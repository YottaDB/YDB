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

#include <stddef.h>		/* for offsetof macro */
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"		/* for the FILE_INFO macros */
#include "jnl.h"
#include "iosp.h"
#include "efn.h"		/* for efn_immed_wait and efn_ignore */
#include "gdsbgtr.h"		/* for the BG_TRACE_PRO_ANY macros */
#include "timers.h"		/* for TIM_AST_WAIT */
#include "wcs_phase2_commit_wait.h"

#define	MAX_DBSYNC_DEFERS	10	/* 10 times deferring of 5 sec (TIM_DEFER_DBSYNC) each for a total of 50 seconds */
#define MAX_DBSYNC_LOOPS	600	/* each loop is of 5msec and we wait for a max. of 30 seconds */

#ifdef GTM_MALLOC_RENT
#	define	GTM_MALLOC_NO_RENT_ONLY(X)
#else
#	define	GTM_MALLOC_NO_RENT_ONLY(X)	X
#endif

GBLDEF	int4			defer_dbsync[2] = { TIM_DEFER_DBSYNC, -1 };	/* picked from timers.h */

GBLREF	gd_region		*gv_cur_region;
GBLREF	int4			wtfini_in_prog;
GBLREF	short			astq_dyn_avail;
GBLREF	volatile int4		gtmMallocDepth;		/* Recursion indicator */
GBLREF	uint4			process_id;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	volatile int4		fast_lock_count;
GBLREF	volatile int4		crit_count;
GBLREF	volatile boolean_t	in_mutex_deadlock_check;

error_def(ERR_JNLFLUSH);
error_def(ERR_TEXT);

/* Sync the filehdr (and epoch in the journal file if before imaging). The goal is to sync the database,
 * but if we find us in a situation where we need to block on someone else, then we defer this to the next round.
 * This is the equivalent of the Unix wcs_clean_dbsync() routine.
 */
void	wcs_clean_dbsync_ast(sgmnt_addrs *csa)
{
	static readonly int4	pause[2] = { TIM_AST_WAIT, -1 };	/* picked from wcs_timer_start */
	boolean_t		bimg_jnl, dbsync_defer_timer;		/* bimg_jnl --> before-imaging or not */
	cache_que_head		*crqwip;
	int			counter, status;
	gd_region		*reg;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	node_local_ptr_t	cnl;
	sgmnt_addrs		*save_csa;
	sgmnt_data_ptr_t	csd;
	void			fileheader_sync();
	uint4			jnl_status;

	assert(lib$ast_in_prog());	/* If dclast fails and setast is used, this assert trips, but in that
					 * case, we anyway want to know why we needed setast. */
	/* Although csa->dbsync_timer is almost always TRUE if here, there is a small possibility it is FALSE. This is
	 * possible if we are currently in gds_rundown for this region where the flag is reset to FALSE irrespective
	 * of whether we have a pending timer or a sys$qio-termination-signalling-ast. In the case dbsync_timer is
	 * FALSE, return. There is a very remote possibility that we miss syncing the db if the qio of the last
	 * dirty buffer finishes after we die and we are not the last writer. In this case the sync won't be done
	 * since all the maintenance is process-private. But that possibility is too remote and we will live with
	 * it for now since otherwise we need to implement grander mechanisms involving shared memory and the like.
	 */
	reg = csa->region;
	assert(FALSE == csa->dbsync_timer || reg->open);
	/* Don't know how this can happen, but if region is closed, just return in PRO */
	/* In MM, not yet sure whether it will work */
	if (FALSE == csa->dbsync_timer || dba_mm == reg->dyn.addr->acc_meth || !reg->open)
	{
		csa->dbsync_timer = FALSE;
		astq_dyn_avail++;
		return;
	}
	/* Save to see if we are in crit anywhere */
	save_csa = ((NULL == gv_cur_region || FALSE == gv_cur_region->open) ?  NULL : (&FILE_INFO(gv_cur_region)->s_addrs));
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	jpc = csa->jnl;
	BG_TRACE_PRO_ANY(csa, n_dbsync_timers);
	assert(!JNL_ALLOWED(csd) || NULL != jpc);

	/* Note that even if the active queue was emptied when this routine was called, due to
	 * concurrent update activity, cnl->wcs_active_lvl can be non-zero when we reach here. We
	 * defer syncing the database in this case to the next time the active queue becomes empty or
	 * when we reach the next scheduled epoch_time (only if before-imaging) whichever is earlier.
	 */
	dbsync_defer_timer = FALSE;
	if (!cnl->wcs_active_lvl)
	{	/* Currently VMS timer writes don't have the optimizations for deferring expensive IO at
		 * critical times that exist in Unix. Need to get them (those that apply) to VMS too. They are
		 *   1) We are in the midst of lseek/read/write IO. This could reset an lseek. (Doesn't apply to VMS).
		 *   2) We are aquiring/releasing crit in any region (Strictly speaking it is enough
		 *		to check this in the current region, but doesn't harm us much).
		 *	Note that the function "mutex_deadlock_check" resets crit_count to 0 temporarily even though we
		 *	might actually be in the midst of acquiring crit. Therefore we should not interrupt mainline code
		 *	if we are in the "mutex_deadlock_check" as otherwise it presents reentrancy issues.
		 *   3) We have crit in the current region OR are in the middle of commit for this region (even though
		 *	we dont hold crit) OR are in wcs_wtstart (potentially holding write interlock and keeping another
		 *	process in crit waiting) OR we need to wait to obtain crit.
		 *   4) We are in a "fast lock".
		 * Out of the above, items (2) & (3) are currently being taken care of below since they can cause
		 *	deadlocks (if not taken care of) while the others are just performance enhancements. Note
		 *	that the last part of (3) is taken care of by doing a grab_crit_immediate() rather than a grab_crit().
		 * Also to be taken care of are the following situations.
		 *   1) We are currently in wcs_wtfini be it the same or a different region.
		 *	To avoid reentrancy issues (if same region) and deadlock issues (if different region).
		 *   2) We are currently in malloc(). Although nested malloc() now works and we won't be needing it
		 *	as much, want to be paranoid here since there are quite a few functions called from here.
		 * Other reentrancy issues to be taken care of are
		 *   1) Avoid doing recursive wcs_recovers.
		 */
		dbsync_defer_timer = TRUE;
		crqwip = &csa->acc_meth.bg.cache_state->cacheq_wip;
		if (!mupip_jnl_recover && 0 == crit_count && !in_mutex_deadlock_check && !wtfini_in_prog && !fast_lock_count
			GTM_MALLOC_NO_RENT_ONLY(&& 0 == gtmMallocDepth)
			&& ((NULL == save_csa) || !T_IN_CRIT_OR_COMMIT_OR_WRITE(save_csa))
			&& !T_IN_CRIT_OR_COMMIT_OR_WRITE(csa)
			&& (TRUE == grab_crit_immediate(reg)))
		{	/* Note that if we are here, we have obtained crit using grab_crit_immediate. Also grab_crit_immediate
			 * doesn't call wcs_recover if wc_blocked is TRUE in order to prevent possible deadlocks.
			 * Note that mutex_lockwim() cannot be used since crit_count is not maintained there.
			 */
			assert(csa->ti->early_tn == csa->ti->curr_tn);
			/* if wcs_wtfini() returns FALSE, it means the cache is suspect. but we are in interrupt code
			 * and therefore want to play it safe. Hence we will not set wc_blocked. we will defer writing
			 * epoch and wait for a future call to mainline code to detect this and initiate cache recovery.
			 */
			/* Wait for ALL active phase2 commits to complete first. If they dont complete in time then defer
			 * writing the epoch. Also dont wait if cnl->wc_blocked is already set to TRUE. In that case
			 * defer writing the EPOCH unconditionally. */
			if (!cnl->wc_blocked && (!cnl->wcs_phase2_commit_pidcnt || wcs_phase2_commit_wait(csa, NULL))
				&& wcs_wtfini(reg)) /* wcs_wtfini handles calls from ASTs appropriately */
			{
				if (JNL_ENABLED(csd))
				{
					jb = jpc->jnl_buff;
					if (jb->before_images)
						bimg_jnl = TRUE;
				} else
					bimg_jnl = FALSE;
				/* Note that if before-imaging and we haven't opened the journal file, then we
				 * can't write an epoch record here because opening the jnl file involves a
				 * heavyweight routine jnl_file_open() which is risky in this ast-prone code.
				 * Also, if before-imaging and the journal file has been switched since the time the
				 * dbsync timer started, we do not want to do any writes as they will go to the older
				 * generation journal file. It is ok not to write an EPOCH record in the older generation
				 * journal file because whichever process did the journal file switch would have done
				 * exactly that. And therefore there is no need to start a new dbsync timer in this case.
				 */
				if (cnl->wcs_active_lvl || bimg_jnl && ((NOJNL == jpc->channel) || JNL_FILE_SWITCHED(jpc)))
					dbsync_defer_timer = FALSE;	/* don't/can't write epoch. */
				else if (0 == crqwip->fl)
				{
					if (!bimg_jnl)
					{	/* Entire wip queue is flushed. So sync the file-header now */
						assert(cnl->wc_in_free == csd->n_bts);
						BG_TRACE_PRO_ANY(csa, n_dbsync_writes);
						fileheader_sync(reg);	/* sync the fileheader to disk */
						dbsync_defer_timer = FALSE;
					} else if (jb->dskaddr == jb->freeaddr)
					{	/* Entire wip queue and jnl buffer is flushed. So write an epoch record now. */
						assert(cnl->wc_in_free == csd->n_bts);
						BG_TRACE_PRO_ANY(csa, n_dbsync_writes);
						fileheader_sync(reg);	/* sync the fileheader to disk */
						/* To avoid deadlocks (e.g. we waiting for a jnl_flush while someone
						 * is holding the io_in_prog lock) we use a kludge. Setting jb->blocked
						 * prevents others from picking up the io_in_prog lock. We then check
						 * whether there is anyone holding the lock. If so, we defer writing the
						 * epoch to the next round and if not go ahead with the flush. Note that
						 * "someone" above includes ourselves too since the qio we have done prior
						 * to entering wcs_wipchk_ast will again be delivered as a jnl_qio_end AST
						 * which will again be blocked.
						 */
						jb->blocked = process_id;
						if (!jb->io_in_prog)
						{
							assert(NOJNL != jpc->channel);
							/* Since the journal buffer is flushed to disk at this point
							 * we don't expect any other routines (like jnl_write_attempt etc.)
							 * to be called. Also since the epoch-record is less than a hundred
							 * bytes, we don't expect a jnl_qio_start to be called at the end
							 * of jnl_write(). We also assume that the check for extension of
							 * journal file takes into account space for an epoch + eof + align.
							 * Note that the assert below checks that the min_write_size (the value
							 * needed to trigger a jnl_qio_write) is less than the maximum number of
							 * bytes that will be written in the journal buffer by jnl_write_epoch_rec
							 * (= size of the epoch record + maximum size of align record if needed).
							 */
							/* Is there a correctness issue if the file gets extended? The assumption
							 * about space check for epoch + eof + align may not be correct. Also,
							 * now we may be writing a PINI as well. Vinaya, 2003, May 2. Check with
							 * Narayanan */
							assert(2 * EPOCH_RECLEN + PINI_RECLEN + 3 * MIN_ALIGN_RECLEN <
									jb->min_write_size);
							assert(csa->ti->curr_tn == csa->ti->early_tn);
							/* There is no need for jnl_ensure_open here since we have crit and
							 * have already determined that the journal file has not been switched.
							 */
							/* Initialize gbl_jrec_time if necessary before jnl_put_jrt_pini */
							if (!jgbl.dont_reset_gbl_jrec_time)
								SET_GBL_JREC_TIME;
							/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time (if needed) to
							 * maintain time order of jnl records. This needs to be done BEFORE
							 * writing any records to the journal file.
							 */
							ADJUST_GBL_JREC_TIME(jgbl, jb);
							if (0 == jpc->pini_addr)
							{/* in the rare case when we haven't done any updates to the db (till
							   * now only db reads), but had to flush the jnl buffer and cache due to
							   * lack of cache buffer (flush trigger mechanism in t_qread) we may not
							   * have written our PINI record yet */
								jnl_put_jrt_pini(csa);
							}
							jnl_write_epoch_rec(csa);
							INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_epoch_idle, 1);
							/* Need to flush this epoch record out */
							jnl_status = jnl_flush(reg);	/* handles calls from ASTs appropriately */
							if (SS_NORMAL == jnl_status)
							{
								assert(jb->dskaddr == jb->freeaddr);
								dbsync_defer_timer = FALSE;
								assert(0 == jb->blocked);    /* jnl_flush should have reset this.*/
								if (process_id == jb->blocked)
									jb->blocked = 0;
							} else
							{
								send_msg(VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
									ERR_TEXT, 2,
									RTS_ERROR_TEXT("Error with journal flush in wcsdbsyncast"),
									jnl_status);
								assert(NOJNL == jpc->channel);/* jnl file lost has been triggered */
								/* In this routine, all code that follows from here on does not
								 * assume anything about the journaling characteristics of this
								 * database so it is safe to continue execution even though
								 * journaling got closed in the middle.
								 */
							}
						} else
							jb->blocked = 0;
					} else
						jnl_start_ast(jpc);	/* Start a journal write and defer epoch writing. */
				}
			}
			rel_crit(reg);
		}
	}
	if (FALSE != dbsync_defer_timer)
	{
		for (counter = 0; 1 > astq_dyn_avail; counter++)
		{	/* Wait until we have room to queue our timer AST for wcs_clean_dbsync_ast. */
			assert(FALSE);
			if (SS$_NORMAL == sys$setimr(efn_timer_ast, &pause, 0, 0, 0))
				sys$synch(efn_timer_ast, 0);
			if (counter > MAX_DBSYNC_LOOPS)
			{
				csa->dbsync_timer = FALSE;
				astq_dyn_avail++;
				return;		/* in this case, we skip syncing the db. */
			}
		}
		astq_dyn_avail--;
		if (MAX_DBSYNC_DEFERS > csa->dbsync_timer++)
		{
			status = sys$setimr(efn_ignore, &defer_dbsync[0], wcs_clean_dbsync_ast, csa, 0);
			if (0 == (status & 1))
			{
				assert(FALSE);
				csa->dbsync_timer = FALSE;
				astq_dyn_avail++;	/* in this case too, we skip syncing the db. */
			}
		} else
		{	/* We have deferred the dbsync timer at least MAX_DBSYNC_DEFERS times (nearly 50 seconds). We cannot keep
			 * doing this indefinitely as it is possible that whatever is causing us to defer this timer (crit_count
			 * being non-zero etc.) is in turn blocked because it needs a timer queue entry but cannot find one due
			 * to wcs_clean_dbsync_ast eternally using up the same (eats up the TQELM job/process quota). Therefore
			 * to avoid a potential deadlock, we stop requeueing ourselves even though it means we will skip syncing
			 * the db. The only one that cares for this dbsync is journal recovery which anyways has been worked around
			 * to take care of indefinite deferring (equivalent to skipping the syncing) so that should not be an issue.
			 */
			csa->dbsync_timer = FALSE;	/* in this case, we skip syncing the db. */
		}
	} else
		csa->dbsync_timer = FALSE;
	astq_dyn_avail++;
	return;
}
