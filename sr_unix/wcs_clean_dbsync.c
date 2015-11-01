/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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
#include "tp_grab_crit.h"
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
GBLREF	volatile int4		db_fsync_in_prog, jnl_qio_in_prog;
GBLREF	volatile int4 		fast_lock_count;
GBLREF	volatile int4		gtmMallocDepth;		/* Recursion indicator */
GBLREF	boolean_t	 	mupip_jnl_recover;

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

	csa = *csaptr;
	assert(csa->dbsync_timer);	/* to ensure no duplicate dbsync timers */
	reg = csa->region;
	assert(reg->open);
	/* Don't know how this can happen, but if region is closed, just return in PRO.
	 * Also not sure if it is meaningful in MM since disk syncing is out of our control */
	if (dba_mm == reg->dyn.addr->acc_meth || !reg->open)
	{
		csa->dbsync_timer = FALSE;
		return;
	}
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
             	 *   3) We have crit in the current region or we need to wait to obtain crit.
		 *   	At least one reason why we should not wait to obtain crit is because the timeout mechanism
		 *   	for the critical section is currently (as of 2004 May) driven by heartbeat on Tru64, AIX,
		 *   	Solaris and HPUX. The periodic heartbeat handler cannot pop as it is a SIGALRM
		 *   	handler and cannot nest while we are already in a SIGALRM handler for the wcs_clean_dbsync.
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
		if (!mupip_jnl_recover NOPIO_ONLY(&& (FALSE == lseekIoInProgress_flag))
			GTM_MALLOC_NO_RENT_ONLY(&& 0 == gtmMallocDepth)
			&& (0 == crit_count)       && (0 == fast_lock_count)
			&& (!jnl_qio_in_prog)      && (!db_fsync_in_prog)
		        && (!jpc || !jpc->jnl_buff || (LOCK_AVAILABLE == jpc->jnl_buff->fsync_in_prog_latch.u.parts.latch_pid))
			&& (NULL == check_csaddrs || FALSE == check_csaddrs->now_crit) && (FALSE == csa->now_crit)
			&& (FALSE != tp_grab_crit(reg)))
		{
			/* Note that if we are here, we have obtained crit using tp_grab_crit. */
			assert(csa->ti->early_tn == csa->ti->curr_tn);
			/* Note that the following wcs_flu() asks for an epoch to be synced. But if not before-imaging,
			 * it will just flush the file-header which is exactly what we want in that case.
			 */
			wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
			BG_TRACE_PRO_ANY(csa, n_dbsync_writes);
			dbsync_defer_timer = FALSE;
			rel_crit(reg);
		}
	}
	if (dbsync_defer_timer)
	{
		assert(sizeof(int4) == sizeof(csa));
		start_timer((TID)csa, TIM_DEFER_DBSYNC, &wcs_clean_dbsync, sizeof(csa), (char *)&csa);
	} else
		csa->dbsync_timer = FALSE;
	/* To restore to former glory, don't use TP_CHANGE_REG, 'coz we might mistakenly set cs_addrs and cs_data to NULL
	 * if the region we are restoring to has been closed. Don't use tp_change_reg 'coz we might be ripping out the structures
	 * needed in tp_change_reg in gv_rundown. */
	gv_cur_region = save_region;
	cs_addrs = save_csaddrs;
	cs_data = save_csdata;
	return;
}
