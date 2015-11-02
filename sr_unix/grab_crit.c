/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gdsbgtr.h"
#include "filestruct.h"
#include "send_msg.h"
#include "mutex.h"
#include "wcs_recover.h"
#include "deferred_signal_handler.h"
#include "caller_id.h"
#include "is_proc_alive.h"
#ifdef DEBUG
#include "gtm_stdio.h"
#include "gt_timer.h"
#include "wbox_test_init.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#endif
#include "gtmimagename.h"
#include "error.h"
#include "anticipatory_freeze.h"

GBLREF	volatile int4		crit_count;
GBLREF	short			crash_count;
GBLREF	uint4 			process_id;
GBLREF	node_local_ptr_t	locknl;
#ifdef DEBUG
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnl_gbls_t		jgbl;
#endif
GBLREF	boolean_t		mupip_jnl_recover;

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);
error_def(ERR_DBFLCORRP);

void	grab_crit(gd_region *reg)
{
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	node_local_ptr_t        cnl;
	sgmnt_data_ptr_t	csd;
	enum cdb_sc		status;
	mutex_spin_parms_ptr_t	mutex_spin_parms;
	DEBUG_ONLY(sgmnt_addrs	*jnlpool_csa;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
#	ifdef DEBUG
	if (gtm_white_box_test_case_enabled
		&& (WBTEST_SENDTO_EPERM == gtm_white_box_test_case_number)
		&& (0 == cnl->wbox_test_seq_num))
	{
		FPRINTF(stderr, "MUPIP BACKUP entered grab_crit\n");
		cnl->wbox_test_seq_num = 1;
		while (2 != cnl->wbox_test_seq_num)
			LONG_SLEEP(1);
		FPRINTF(stderr, "MUPIP BACKUP resumed in grab_crit\n");
		cnl->wbox_test_seq_num = 3;
	}
#	endif
	assert(!csa->hold_onto_crit);
	if (!csa->now_crit)
	{
#		ifdef DEBUG
		if (NULL != jnlpool.jnlpool_ctl)
		{	/* We should never request crit on a database region while already holding the lock on the journal pool.
			 * Not following the protocol (obtaining lock on journal pool AFTER obtaining crit on database region),
			 * can lead to potential deadlocks
			 */
			jnlpool_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
			assert(!jnlpool_csa->now_crit);
		}
#		endif
		assert(0 == crit_count);
		crit_count++;	/* prevent interrupts */
		TREF(grabbing_crit) = reg;
		DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		mutex_spin_parms = (mutex_spin_parms_ptr_t)&csd->mutex_spin_parms;
		status = mutex_lockw(reg, mutex_spin_parms, crash_count);
#		ifdef DEBUG
		if (gtm_white_box_test_case_enabled
			&& (WBTEST_SENDTO_EPERM == gtm_white_box_test_case_number)
			&& (1 == cnl->wbox_test_seq_num))
		{
			FPRINTF(stderr, "MUPIP SET entered grab_crit\n");
			cnl->wbox_test_seq_num = 2;
			while (3 != cnl->wbox_test_seq_num)
				LONG_SLEEP(1);
			FPRINTF(stderr, "MUPIP SET resumed in grab_crit\n");
		}
#		endif
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			crit_count = 0;
			TREF(grabbing_crit) = NULL;
			switch(status)
			{
				case cdb_sc_critreset:
					rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
				case cdb_sc_dbccerr:
					rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
				default:
					GTMASSERT;
			}
			return;
		}
		/* There is only one case we know of when cnl->in_crit can be non-zero and that is when a process holding
		 * crit gets kill -9ed and another process ends up invoking "secshr_db_clnup" which in turn clears the
		 * crit semaphore (making it available for waiters) but does not also clear cnl->in_crit since it does not
		 * hold crit at that point. But in that case, the pid reported in cnl->in_crit should be dead. Check that.
		 */
		assert((0 == cnl->in_crit) || (FALSE == is_proc_alive(cnl->in_crit, 0)));
		cnl->in_crit = process_id;
		CRIT_TRACE(crit_ops_gw);	/* see gdsbt.h for comment on placement */
		TREF(grabbing_crit) = NULL;
		crit_count = 0;
	}
	/* Commands/Utilties that plays with the file_corrupt flags (DSE/MUPIP SET -PARTIAL_RECOV_BYPASS/RECOVER/ROLLBACK) should
	 * NOT issue DBFLCORRP. Use skip_file_corrupt_check global variable for this purpose
	 */
	if (csd->file_corrupt && !TREF(skip_file_corrupt_check))
		rts_error(VARLSTCNT(4) ERR_DBFLCORRP, 2, DB_LEN_STR(reg));
	if (cnl->wc_blocked)
		wcs_recover(reg);
	return;
}
