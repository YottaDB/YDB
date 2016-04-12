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

#include "gtm_fcntl.h"	/* needed for silly aix's expansion of open to open64 */
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* for tp_region definition */
#include "gt_timer.h"		/* for TID definition */
#include "timers.h"		/* for TIM_DEFER_DBSYNC #define */
#include "gdsbgtr.h"		/* for the BG_TRACE_PRO macros */
#include "gtmio.h"		/* for the GET_LSEEK_FLAG macro */
#include "wcs_clean_dbsync.h"
#include "wcs_flu.h"
#include "lockconst.h"

#ifdef GTM_MALLOC_RENT
#	define	GTM_MALLOC_NO_RENT_ONLY(X)
#else
#	define	GTM_MALLOC_NO_RENT_ONLY(X)	X
#endif

NOPIO_ONLY(GBLREF boolean_t	*lseekIoInProgress_flags;)	/* needed for the LSEEK* macros in gtmio.h */
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	volatile int4		crit_count;
GBLREF	volatile boolean_t	in_mutex_deadlock_check;
GBLREF	volatile int4		db_fsync_in_prog, jnl_qio_in_prog;
GBLREF	volatile int4 		fast_lock_count;
GBLREF	volatile int4		gtmMallocDepth;		/* Recursion indicator */
GBLREF	boolean_t	 	mupip_jnl_recover;
#ifdef DEBUG
GBLREF	unsigned int		t_tries;
GBLREF	volatile boolean_t	timer_in_handler;
#endif

/* Sync the filehdr (and epoch in the journal file if before imaging). The goal is to sync the database,
 * but if we find us in a situation where we need to block on someone else, then we defer this to the next round.
 */
void	wcs_clean_dbsync(TID tid, int4 hd_len, sgmnt_addrs **csaptr)
{
	boolean_t		dbsync_defer_timer;
	gd_region               *reg, *save_region;
	jnl_private_control	*jpc;
	node_local_ptr_t	cnl;
	sgmnt_addrs		*csa, *check_csaddrs, *save_csaddrs;
	sgmnt_data_ptr_t	csd, save_csdata;
	NOPIO_ONLY(boolean_t	lseekIoInProgress_flag;)
	DEBUG_ONLY(boolean_t	save_ok_to_call_wcs_recover;)
	boolean_t		is_mm;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = *csaptr;
	assert(timer_in_handler);
	assert(csa->dbsync_timer);	/* to ensure no duplicate dbsync timers */
	CANCEL_DBSYNC_TIMER(csa);	/* reset csa->dbsync_timer now that the dbsync timer has popped */
	assert(!csa->dbsync_timer);
	reg = csa->region;
	/* Don't know how this can happen, but if region is closed, just return in PRO. */
	if (!reg->open)
	{
		assert(FALSE);
		return;
	}
	is_mm = (dba_mm == reg->dyn.addr->acc_meth);
	save_region = gv_cur_region; /* Save for later restore. See notes about restore */
	save_csaddrs = cs_addrs;
	save_csdata = cs_data;
	/* Save to see if we are in crit anywhere */
	check_csaddrs = ((NULL == save_region || FALSE == save_region->open) ?  NULL : (&FILE_INFO(save_region)->s_addrs));
	/* Note the non-usage of TP_CHANGE_REG_IF_NEEDED macros since this routine can be timer driven. */
	TP_CHANGE_REG(reg);
	csd = csa->hdr;
	cnl = csa->nl;
	jpc = csa->jnl;
	BG_TRACE_PRO_ANY(csa, n_dbsync_timers);
	assert(csa == cs_addrs);
	assert(!JNL_ALLOWED(csd) || NULL != jpc);
	/* Note that even if the active queue was emptied when this routine was called, due to
	 * concurrent update activity, cnl->wcs_active_lvl can be non-zero when we reach here. We
	 * defer syncing in this case to the next time the active queue becomes empty ( or when we
	 * reach the next scheduled epoch_time -- in case of before-imaging) whichever is earlier.
	 *
	 * Note that if we are already in wcs_wtstart for this region, then invoking wcs_flu() won't
	 * recurse on wcs_wtstart. In any case the interrupted wcs_wtstart invocation will take care
	 * of the dbsync_timer once it is done. Therefore in this case too no need to do the dbsync.
	 */
	dbsync_defer_timer = FALSE;
	if (!cnl->wcs_active_lvl && !csa->in_wtstart)
	{	/* Similar to wcs_stale, defer expensive IO flushing if any of the following is true.
		 *   1) We are in the midst of lseek/read/write IO. This could reset an lseek.
		 *   2) We are aquiring/releasing crit in any region (Strictly speaking it is enough
		 *		to check this in the current region, but doesn't harm us much).
		 *	Note that the function "mutex_deadlock_check" resets crit_count to 0 temporarily even though we
		 *	might actually be in the midst of acquiring crit. Therefore we should not interrupt mainline code
		 *	if we are in the "mutex_deadlock_check" as otherwise it presents reentrancy issues.
		 *   3) We have crit in the current region OR are in the middle of commit for this region (even though
		 *	we dont hold crit) OR are in wcs_wtstart (potentially holding write interlock and keeping another
		 *	process in crit waiting) OR we need to wait to obtain crit. At least one reason why we should not wait
		 *	to obtain crit is because the timeout mechanism for the critical section is currently (as of 2004 May)
		 *	driven by heartbeat on Tru64, AIX, Solaris and HPUX. The periodic heartbeat handler cannot pop as
		 *	it is a SIGALRM handler and cannot nest while we are already in a SIGALRM handler for the wcs_clean_dbsync.
		 *   	Were this to happen, we could end up waiting for crit, not being able to interrupt the wait
		 *   	with a timeout resulting in a hang until crit became available.
		 *   4) We are in a "fast lock".
		 *   5) We are in gtm_malloc. Don't want to recurse on malloc.
		 * Other deadlock causing conditions that need to be taken care of
		 *   1) We already have either the fsync_in_prog or the io_in_prog lock.
		 *   2) We are currently doing a db_fsync on some region.
		 */
		dbsync_defer_timer = TRUE;
		GET_LSEEK_FLAG(FILE_INFO(reg)->fd, lseekIoInProgress_flag);
		DEBUG_ONLY(
			/* We invoke grab_crit_immediate below which can potentially do cache-recoveries if cnl->wc_blocked is set.
			 * But wcs_recover has an assert that we never invoke it in the final retry. This is to avoid
			 * restarts in the final retry. But wcs_clean_dbsync invokes grab_crit_immediate only if we dont already
			 * hold crit and that means we have already finished commit on this particular region (e.g. if
			 * commit is complete on all regions and crit is released on all of them but before we reset t_tries
			 * to 0 in t_end/tp_tend) so it is okay to invoke wcs_recover in that case. Signal that to wcs_recover
			 * by setting ok_to_call_wcs_recover to TRUE. Need to save and restore the global as it could be
			 * TRUE or FALSE depending on where wcs_clean_dbsync interrupted mainline code.
			 */
			assert(CDB_STAGNATE >= t_tries || WBTEST_ENABLED(WBTEST_ANTIFREEZE_GVDATAFAIL));
			if (CDB_STAGNATE <= t_tries)
			{
				save_ok_to_call_wcs_recover = TREF(ok_to_call_wcs_recover);
				TREF(ok_to_call_wcs_recover) = TRUE;
			}
		)
		if (!mupip_jnl_recover NOPIO_ONLY(&& (FALSE == lseekIoInProgress_flag))
			GTM_MALLOC_NO_RENT_ONLY(&& 0 == gtmMallocDepth)
			&& (0 == crit_count) && !in_mutex_deadlock_check
			&& (0 == fast_lock_count)
			&& (!jnl_qio_in_prog)      && (!db_fsync_in_prog)
			&& (!jpc || !jpc->jnl_buff || (LOCK_AVAILABLE == jpc->jnl_buff->fsync_in_prog_latch.u.parts.latch_pid))
			&& ((NULL == check_csaddrs) || !T_IN_CRIT_OR_COMMIT_OR_WRITE(check_csaddrs))
			&& !T_IN_CRIT_OR_COMMIT_OR_WRITE(csa)
			&& (FALSE != grab_crit_immediate(reg)))
		{	/* Note that grab_crit_immediate invokes wcs_recover in case cnl->wc_blocked is non-zero.  This means we
			 * could be doing cache recovery even though we are in interrupt code.  If this is found undesirable, the
			 * logic in grab_crit_immediate that invokes wcs_recover has to be re-examined.
			 */
			/* Note that if we are here, we have obtained crit using grab_crit_immediate. */
			assert(csa->ti->early_tn == csa->ti->curr_tn);
			/* Do not invoke wcs_flu if the database has a newer journal file than what this process had open
			 * when the dbsync timer was started in wcs_wtstart. This is because mainline (non-interrupt) code
			 * in jnl_write_attempt/jnl_output_sp assumes that interrupt code will not update jpc structures to
			 * point to latest journal file (i.e. will not do a jnl_ensure_open) but wcs_flu might invoke just
			 * that. It is ok not to do a wcs_flu since whichever process did the journal switch would have
			 * written the EPOCH record in the older generation journal file. Therefore there is no need to
			 * start a new dbsync timer in this case.
			 *
			 * If journaling and writing EPOCHs, do a wcs_flu only if there has been at least one transaction
			 * since the last time someone wrote an EPOCH.
			 *
			 * If NOT journaling or if NOT writing EPOCHs, do a wcs_flu only if there has been at least one
			 * transaction since the last time someone did a wcs_flu.
			 *
			 * This way wcs_flu is not redundantly invoked and it ensures that the least number of epochs
			 * (only the necessary ones) are written OR the least number of db file header flushes are done.
			 *
			 * If MM and not writing EPOCHs, we need to flush the fileheader out as that is not mmap'ed.
			 */
			/* Write idle/free epoch only if db curr_tn did not change since when the last dirty cache record was
			 * written in wcs_wtstart to when the dbsync timer (5 seconds) popped. If the curr_tn changed it means
			 * some other update happened in between and things are no longer idle so the previous idle dbsync
			 * timer can be stopped. A new timer will be written when the later updates finish and leave the db
			 * idle again. Note that there are some race conditions where we might not be accurate in writing idle
			 * EPOCH only when necessary (since we dont hold crit at the time we record csa->dbsync_timer_tn). But
			 * any error will always be on the side of caution so we might end up writing more idle EPOCHs than
			 * necessary. Also, even if we dont write an idle EPOCH (for example because we found an update
			 * happened later but that update turned out to be a duplicate SET which will not start an idle
			 * EPOCH timer), journal recovery already knows to handle the case where an idle EPOCH did not get
			 * written. So things will still work but it might just take a little longer than usual.
			 */
			if (csa->dbsync_timer_tn == csa->ti->curr_tn)
			{	/* Note that it is possible in rare cases that an online rollback took csa->ti->curr_tn back
				 * and the exact # of updates happened concurrently to take csa->ti->curr_tn back to where it
				 * was to match csa->dbsync_timer_tn. In this case, we will be writing an epoch unnecessarily
				 * but this is a very rare situation that is considered okay to write the epoch in that case
				 * as it keeps the if check simple for the most frequent path.
				 */
				if ((NULL != jpc) && JNL_HAS_EPOCH(jpc->jnl_buff)
					? (((NOJNL == jpc->channel) || !JNL_FILE_SWITCHED(jpc))
							&& (jpc->jnl_buff->epoch_tn < csa->ti->curr_tn))
					: (cnl->last_wcsflu_tn < csa->ti->curr_tn))
				{
					wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH | WCSFLU_CLEAN_DBSYNC
							| WCSFLU_SPEEDUP_NOBEFORE);
					BG_TRACE_PRO_ANY(csa, n_dbsync_writes);
					/* If MM, file could have been remapped by wcs_flu above.
					 * If so, cs_data needs to be reset.
					 */
					if (is_mm && (save_csaddrs == cs_addrs) && (save_csdata != cs_data))
						save_csdata = cs_addrs->hdr;
				}
			}
			dbsync_defer_timer = FALSE;
			assert(!csa->hold_onto_crit); /* this ensures we can safely do unconditional rel_crit */
			rel_crit(reg);
		}
		DEBUG_ONLY(
			if (CDB_STAGNATE <= t_tries)
				TREF(ok_to_call_wcs_recover) = save_ok_to_call_wcs_recover;
		)
	}
	if (dbsync_defer_timer)
	{
		assert(SIZEOF(INTPTR_T) == SIZEOF(csa));
		/* Adding a new dbsync timer should typically be done in a deferred zone to avoid duplicate timer additions for the
		 * same TID. But, in this case, we are guaranteed that timers won't pop as we are already in a timer handler. As
		 * for the external interrupts, they should be okay to interrupt at this point since, unlike timer interrupts,
		 * control won't return to mainline code. So, in either case, we can safely add the new timer.
		 */
		if (!csa->dbsync_timer)
			START_DBSYNC_TIMER(csa, TIM_DEFER_DBSYNC);
	}
	/* To restore to former glory, don't use TP_CHANGE_REG, 'coz we might mistakenly set cs_addrs and cs_data to NULL
	 * if the region we are restoring to has been closed. Don't use tp_change_reg 'coz we might be ripping out the structures
	 * needed in tp_change_reg in gv_rundown. */
	gv_cur_region = save_region;
	cs_addrs = save_csaddrs;
	cs_data = save_csdata;
	return;
}
