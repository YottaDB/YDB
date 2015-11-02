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

#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mutex.h"
#include "deferred_signal_handler.h"
#include "caller_id.h"
#include "is_proc_alive.h"

GBLREF	volatile int4		crit_count;
GBLREF	uint4			process_id;
GBLREF	node_local_ptr_t	locknl;
GBLREF	boolean_t		hold_onto_locks;

/* Note about usage of this function : Create dummy gd_region, gd_segment, file_control,
 * unix_db_info, sgmnt_addrs, and allocate mutex_struct (and NUM_CRIT_ENTRY * mutex_que_entry),
 * mutex_spin_parms_struct, and node_local in shared memory. Initialize the fields as in
 * jnlpool_init(). Pass the address of the dummy region as argument to this function.
 */
void	grab_lock(gd_region *reg)
{
	unix_db_info 		*udi;
	sgmnt_addrs  		*csa;
	enum cdb_sc		status;
	mutex_spin_parms_ptr_t	mutex_spin_parms;

	error_def(ERR_DBCCERR);
	error_def(ERR_CRITRESET);

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!hold_onto_locks && !csa->hold_onto_crit);
	if (!csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;	/* prevent interrupts */
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		mutex_spin_parms = (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)csa->critical + CRIT_SPACE);
			/* This assumes that mutex_spin_parms_t is located immediately after the crit structures */
		/* As of 10/07/98, crashcnt field in mutex_struct is not changed by any function for the dummy  region */
		status = mutex_lockw(reg, mutex_spin_parms, 0);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			crit_count = 0;
			switch(status)
			{
				case cdb_sc_critreset: /* As of 10/07/98, this return value is not possible */
					rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
				case cdb_sc_dbccerr:
					rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
				default:
					GTMASSERT;
			}
			return;
		}
		/* There is only one case we know of when csa->nl->in_crit can be non-zero and that is when a process holding
		 * crit gets kill -9ed and another process ends up invoking "secshr_db_clnup" which in turn clears the
		 * crit semaphore (making it available for waiters) but does not also clear csa->nl->in_crit since it does not
		 * hold crit at that point. But in that case, the pid reported in csa->nl->in_crit should be dead. Check that.
		 */
		assert((0 == csa->nl->in_crit) || (FALSE == is_proc_alive(csa->nl->in_crit, 0)));
		csa->nl->in_crit = process_id;
		CRIT_TRACE(crit_ops_gw);		/* see gdsbt.h for comment on placement */
		crit_count = 0;
	}
	return;
}
