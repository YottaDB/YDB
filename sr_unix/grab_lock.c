/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "jnl.h"
#include "gtmimagename.h"	/* for IS_GTCM_GNP_SERVER_IMAGE */
#include "anticipatory_freeze.h"
#include "util.h"		/* for OUT_BUFF_SIZE */

GBLREF	volatile int4		crit_count;
GBLREF	uint4			process_id;
GBLREF	node_local_ptr_t	locknl;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		jnlpool_init_needed;

error_def(ERR_DBCCERR);
error_def(ERR_CRITRESET);
error_def(ERR_REPLREQROLLBACK);
error_def(ERR_TEXT);

/* Note about usage of this function : Create dummy gd_region, gd_segment, file_control,
 * unix_db_info, sgmnt_addrs, and allocate mutex_struct (and NUM_CRIT_ENTRY * mutex_que_entry),
 * mutex_spin_parms_struct, and node_local in shared memory. Initialize the fields as in
 * jnlpool_init(). Pass the address of the dummy region as argument to this function.
 */
boolean_t grab_lock(gd_region *reg, boolean_t is_blocking_wait, uint4 onln_rlbk_action)
{
	unix_db_info 		*udi;
	sgmnt_addrs		*csa;
	enum cdb_sc		status;
	mutex_spin_parms_ptr_t	mutex_spin_parms;
	char			scndry_msg[OUT_BUFF_SIZE];
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!csa->hold_onto_crit);
	assert(!csa->now_crit);
	if (!csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;	/* prevent interrupts */
		DEBUG_ONLY(locknl = csa->nl);	/* for DEBUG_ONLY LOCK_HIST macro */
		mutex_spin_parms = (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SPACE);
		/* This assumes that mutex_spin_parms_t is located immediately after the crit structures */
		/* As of 10/07/98, crashcnt field in mutex_struct is not changed by any function for the dummy  region */
		if (is_blocking_wait)
			status = mutex_lockw(reg, mutex_spin_parms, 0);
		else
			status = mutex_lockwim(reg, mutex_spin_parms, 0);
		DEBUG_ONLY(locknl = NULL);	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			crit_count = 0;
			switch(status)
			{
				case cdb_sc_critreset: /* As of 10/07/98, this return value is not possible */
					rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
				case cdb_sc_dbccerr:
					rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
				case cdb_sc_nolock:
					return FALSE;
				default:
					GTMASSERT;
			}
			return FALSE;
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
		if (jnlpool.repl_inst_filehdr->file_corrupt && !jgbl.onlnrlbk)
		{	/* Journal pool indicates an abnormally terminated online rollback. Cannot continue until the rollback
			 * command is re-run to bring the journal pool/file and instance file to a consistent state.
			 */
			SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Instance file header has file_corrupt field set to TRUE");
			/* No need to do rel_lock before rts_error (mupip_exit_handler will do it for us) */
			rts_error(VARLSTCNT(8) ERR_REPLREQROLLBACK, 2, LEN_AND_STR(udi->fn), ERR_TEXT, 2, LEN_AND_STR(scndry_msg));
		}
		/* If ASSERT_NO_ONLINE_ROLLBACK, then no concurrent online rollbacks can happen at this point. So, the jnlpool
		 * should be in in sync. There are two exceptions. If this is GT.CM GNP Server and the last client disconnected, the
		 * server invokes gtcmd_rundown which in-turn invokes gds_rundown thereby running down all active databases at this
		 * point but leaves the journal pool up and running. Now, if an online rollback is attempted, it increments the
		 * onln_rlbk_cycle in the journal pool, but csa->onln_rlbk_cycle is not synced yet. So, the grab_crit done in t_end
		 * will NOT detect a concurrent online rollback and it doesn't need to because the rollback happened AFTER the
		 * rundown. Assert that this is the only case we know of for the cycles to be out-of-sync. In PRO
		 * jnlpool_ctl->onln_rlbk_cycle is used only by the replication servers (which GT.CM is not) and so even if it
		 * continues with an out-of-sync csa->onln_rlbk_cycle, t_end logic does the right thing. The other exception is if
		 * GT.M initialized journal pool while opening database (belonging to a different instance) in gvcst_init (for
		 * anticipatory freeze) followed by an online rollback which increments the jnlpool_ctl->onln_rlbk_cycle but leaves
		 * the repl_csa->onln_rlbk_cycle out-of-sync. At this point, if a replicated database is open for the first time,
		 * we'll reach t_end to commit the update but will end up failing the below assert due to the out-of-sync
		 * onln_rlbk_cycle. So, assert accordingly. Note : even though the cycles are out-of-sync they are not an issue for
		 * GT.M because it always relies on the onln_rlbk_cycle from csa->nl and not from repl_csa. But, we don't remove the
		 * assert as it is valuable for replication servers (Source, Receiver and Update Process).
		 */
		assert((ASSERT_NO_ONLINE_ROLLBACK != onln_rlbk_action)
		       || (csa->onln_rlbk_cycle == jnlpool.jnlpool_ctl->onln_rlbk_cycle) || IS_GTCM_GNP_SERVER_IMAGE
		       || (jnlpool_init_needed && ANTICIPATORY_FREEZE_AVAILABLE));
		if ((HANDLE_CONCUR_ONLINE_ROLLBACK == onln_rlbk_action)
		    && (csa->onln_rlbk_cycle != jnlpool.jnlpool_ctl->onln_rlbk_cycle))
		{
			assert(is_src_server);
			SYNC_ONLN_RLBK_CYCLES;
			gtmsource_onln_rlbk_clnup(); /* side-effect : sets gtmsource_state */
			rel_lock(reg); /* caller knows to disconnect and re-establish the connection */
		}
	}
	return TRUE;
}
