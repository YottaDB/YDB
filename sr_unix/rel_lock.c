/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmsiginfo.h"
#include "mutex.h"
#include "deferred_signal_handler.h"
#include "have_crit.h"
#include "caller_id.h"

GBLREF	volatile int4		crit_count;
GBLREF	uint4 			process_id;
GBLREF	volatile int		suspend_status;
GBLREF	node_local_ptr_t	locknl;
GBLREF	volatile int4           gtmMallocDepth;         /* Recursion indicator */
GBLREF	boolean_t		hold_onto_locks;

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

	error_def(ERR_CRITRESET);
	error_def(ERR_DBCCERR);

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!hold_onto_locks && !csa->hold_onto_crit);
	if (csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;	/* prevent interrupts */
		assert(csa->nl->in_crit == process_id || csa->nl->in_crit == 0);
		CRIT_TRACE(crit_ops_rw);		/* see gdsbt.h for comment on placement */
		csa->nl->in_crit = 0;
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		/* As of 10/07/98, crashcnt field in mutex_struct is not changed by any function for the dummy  region */
		status = mutex_unlockw(reg, 0);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			csa->now_crit = FALSE;
			crit_count = 0;
			if (status == cdb_sc_critreset)
				rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
			else
			{
				assert(status == cdb_sc_dbccerr);
				rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
			}
			return;
		}
		crit_count = 0;
	} else
	{
		CRIT_TRACE(crit_ops_nocrit);
	}
	/* Now that crit for THIS region is released, check if deferred signal/exit handling can be done and if so do it */
	DEFERRED_EXIT_HANDLING_CHECK;
	if ((DEFER_SUSPEND == suspend_status) && OK_TO_INTERRUPT)
		suspend();
}
