/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"	/* for VSIG_ATOMIC_T type */

#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmsiginfo.h"
#include "mutex.h"
#include "deferred_exit_handler.h"
#include "have_crit.h"
#include "caller_id.h"
#include "jnl.h"

GBLREF	volatile int4		crit_count;
GBLREF	uint4 			process_id;
GBLREF	node_local_ptr_t	locknl;
GBLREF	jnl_gbls_t		jgbl;

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);

/* Note about usage of this function : Create dummy gd_region, gd_segment, file_control,
 * unix_db_info, sgmnt_addrs, and allocate mutex_struct (and NUM_CRIT_ENTRY * mutex_que_entry),
 * mutex_spin_parms_struct, and node_local in shared memory. Initialize the fields as in
 * jnlpool_init(). Pass the address of the dummy region as argument to this function.
 */
void	rel_lock(gd_region *reg)
{
	unix_db_info 		*udi;
	sgmnt_addrs  		*csa;
	enum cdb_sc		status;
	intrpt_state_t		prev_intrpt_state;

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	/* Assert that we never come into rel_lock with hold_onto_crit being TRUE. The only exception is for online rollback
	 * when it is done with the actual rollback and is now in mur_close_files to release crit. At this point it will have
	 * hold_onto_crit set to TRUE.
	 */
	assert(!csa->hold_onto_crit || (process_exiting && jgbl.onlnrlbk));
	if (csa->now_crit)
	{
		DEFER_INTERRUPTS(INTRPT_IN_CRIT_FUNCTION, prev_intrpt_state);
		assert(csa->nl->in_crit == process_id || csa->nl->in_crit == 0);
		CRIT_TRACE(csa, crit_ops_rw);		/* see gdsbt.h for comment on placement */
		csa->nl->in_crit = 0;
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		/* As of 10/07/98, crashcnt field in mutex_struct is not changed by any function for the dummy  region */
		status = mutex_unlockw(reg, 0);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			csa->now_crit = FALSE;
			ENABLE_INTERRUPTS(INTRPT_IN_CRIT_FUNCTION, prev_intrpt_state);
			if (status == cdb_sc_critreset)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
			else
			{
				assert(status == cdb_sc_dbccerr);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
			}
			return;
		}
		ENABLE_INTERRUPTS(INTRPT_IN_CRIT_FUNCTION, prev_intrpt_state);
	} else
		CRIT_TRACE(csa, crit_ops_nocrit);
}
