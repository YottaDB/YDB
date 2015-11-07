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

#include <sys/mman.h>
#include <sys/shm.h>

#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_unistd.h"	/* DB_FSYNC needs this */

#include "aswp.h"	/* for ASWP */
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "gdsbgtr.h"
#include "jnl.h"
#include "lockconst.h"		/* for LOCK_AVAILABLE */
#include "interlock.h"
#include "sleep_cnt.h"
#include "performcaslatchcheck.h"
#include "send_msg.h"
#include "gt_timer.h"
#include "is_file_identical.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "wcs_recover.h"
#include "wcs_phase2_commit_wait.h"
#include "wbox_test_init.h"
#include "wcs_mm_recover.h"
#include "memcoherency.h"
#include "gtm_c_stack_trace.h"
#include "anticipatory_freeze.h"
#include "eintr_wrappers.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		process_id;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	volatile int4	db_fsync_in_prog;	/* for DB_FSYNC macro usage */
GBLREF 	jnl_gbls_t	jgbl;
GBLREF 	bool		in_backup;
#ifdef DEBUG
GBLREF	boolean_t	in_mu_rndwn_file;
GBLREF	boolean_t	mupip_jnl_recover;
#endif

error_def(ERR_DBFILERR);
error_def(ERR_DBFSYNCERR);
error_def(ERR_DBIOERR);
error_def(ERR_GBLOFLOW);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLFLUSH);
error_def(ERR_OUTOFSPACE);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_WAITDSKSPACE);
error_def(ERR_WCBLOCKED);
error_def(ERR_WRITERSTUCK);

#define	JNL_WRITE_EPOCH_REC(CSA, CNL, CLEAN_DBSYNC)					\
{											\
	jnl_write_epoch_rec(CSA);							\
	/* Note: Cannot easily use ? : syntax below as INCR_GVSTATS_COUNTER macro	\
	 * is not an arithmetic expression but a sequence of statements.		\
	 */										\
	if (!CLEAN_DBSYNC)								\
	{										\
		INCR_GVSTATS_COUNTER(CSA, CNL, n_jrec_epoch_regular, 1);		\
	} else										\
		INCR_GVSTATS_COUNTER(CSA, CNL, n_jrec_epoch_idle, 1);			\
}

#define	WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(FIX_IN_WTSTART, WAS_CRIT)							\
{															\
	GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, (cnl->in_wtstart), 1);						\
	if (WRITERS_ACTIVE(cnl))											\
	{														\
		DEBUG_ONLY(int4	in_wtstart;) 		/* temporary for debugging purposes */				\
		DEBUG_ONLY(int4	intent_wtstart;) 	/* temporary for debugging purposes */				\
															\
		assert(csa->now_crit);											\
		SIGNAL_WRITERS_TO_STOP(cnl);		/* to stop all active writers */				\
		lcnt = 0;												\
		do													\
		{													\
			DEBUG_ONLY(in_wtstart = cnl->in_wtstart;)							\
			DEBUG_ONLY(intent_wtstart = cnl->intent_wtstart;)						\
			GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, lcnt, (MAXGETSPACEWAIT * 2) - 1);		\
			GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, cnl->wtstart_pid[0], process_id);		\
			if (MAXGETSPACEWAIT DEBUG_ONLY( * 2) == ++lcnt)							\
			{	/* We have noticed the below assert to fail occasionally on some platforms (mostly	\
				 * AIX and Linux). We suspect it is because of waiting for another writer that is 	\
				 * in jnl_fsync (as part of flushing a global buffer) which takes more than a minute	\
				 * to finish. To avoid false failures (where the other writer finishes its job in	\
				 * a little over a minute) we wait for twice the time in the debug version.		\
				 */											\
				GET_C_STACK_MULTIPLE_PIDS("WRITERSTUCK", cnl->wtstart_pid, MAX_WTSTART_PID_SLOTS, 1);	\
				assert((gtm_white_box_test_case_enabled) && 						\
				(WBTEST_BUFOWNERSTUCK_STACK == gtm_white_box_test_case_number));			\
				cnl->wcsflu_pid = 0;									\
				SIGNAL_WRITERS_TO_RESUME(cnl);								\
				if (!WAS_CRIT)										\
					rel_crit(gv_cur_region);							\
				/* Disable white box testing after the first time the					\
				WBTEST_BUFOWNERSTUCK_STACK mechanism has kicked in. This is because as			\
				part of the exit handling process, the control once agin comes to wcs_flu		\
				and at that time we do not want the WBTEST_BUFOWNERSTUCK_STACK white box		\
				mechanism to kick in.*/									\
				GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, gtm_white_box_test_case_enabled, FALSE);	\
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_WRITERSTUCK, 3, cnl->in_wtstart,		\
						DB_LEN_STR(gv_cur_region));						\
				return FALSE;										\
			}												\
			if (-1 == shmctl(udi->shmid, IPC_STAT, &shm_buf))						\
			{												\
				save_errno = errno;									\
				if (1 == lcnt)										\
				{											\
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2,				\
							DB_LEN_STR(gv_cur_region));					\
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_SYSCALL, 5,				\
							RTS_ERROR_LITERAL("shmctl()"), CALLFROM, save_errno);		\
				} 											\
			} else if (1 == shm_buf.shm_nattch)								\
			{												\
				assert((FALSE == csa->in_wtstart) && (0 <= cnl->in_wtstart));				\
				cnl->in_wtstart = 0;	/* fix improper value of in_wtstart if you are standalone */	\
				FIX_IN_WTSTART = TRUE;									\
				cnl->intent_wtstart = 0;/* fix improper value of intent_wtstart if standalone */	\
			} else												\
				wcs_sleep(lcnt);		/* wait for any in wcs_wtstart to finish */		\
		} while (WRITERS_ACTIVE(cnl));										\
		SIGNAL_WRITERS_TO_RESUME(cnl);										\
	}														\
}

#define REL_CRIT_BEFORE_RETURN			\
{						\
	cnl->wcsflu_pid = 0;			\
	if (!was_crit)				\
		rel_crit(gv_cur_region);	\
}

boolean_t wcs_flu(uint4 options)
{
	bool			success, was_crit;
	boolean_t		fix_in_wtstart, flush_hdr, jnl_enabled, sync_epoch, write_epoch, need_db_fsync, in_commit;
	boolean_t		flush_msync, speedup_nobefore, clean_dbsync, return_early;
	unsigned int		lcnt, pass;
	int			save_errno, wtstart_errno;
	jnl_buffer_ptr_t	jb;
	jnl_private_control	*jpc;
	uint4			jnl_status, to_wait, to_msg;
        unix_db_info    	*udi;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	file_control		*fc;
	cache_que_head_ptr_t	crq;
        struct shmid_ds         shm_buf;
	uint4			fsync_dskaddr;

	jnl_status = 0;
	flush_hdr = options & WCSFLU_FLUSH_HDR;
	write_epoch = options & WCSFLU_WRITE_EPOCH;
	sync_epoch = options & WCSFLU_SYNC_EPOCH;
	need_db_fsync = options & WCSFLU_FSYNC_DB;
	flush_msync = options & WCSFLU_MSYNC_DB;
	speedup_nobefore = options & WCSFLU_SPEEDUP_NOBEFORE;
	clean_dbsync = options & WCSFLU_CLEAN_DBSYNC;
	/* WCSFLU_IN_COMMIT bit is set if caller is t_end or tp_tend. In that case, we should NOT invoke wcs_recover if we
	 * encounter an error. Instead we should return the error as such so they can trigger appropriate error handling.
	 * This is necessary because t_end and tp_tend could have pinned one or more cache-records (cr->in_cw_set non-zero)
	 * BEFORE invoking wcs_flu. And code AFTER the wcs_flu in them relies on the fact that those cache records stay
	 * pinned. If wcs_flu invokes wcs_recover, it will reset cr->in_cw_set to 0 for ALL cache-records so code AFTER
	 * the wcs_flu in the caller will fail because no buffer is pinned at that point.
	 */
	in_commit = options & WCSFLU_IN_COMMIT;
	udi = FILE_INFO(gv_cur_region);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	assert(cnl->glob_sec_init);
	/* If called from online rollback, we will have hold_onto_crit set to TRUE with the only exception when called from
	 * gds_rundown in which case process_exiting will be TRUE anyways
	 */
	assert(!jgbl.onlnrlbk || csa->hold_onto_crit || process_exiting);
	assert(mupip_jnl_recover || !csa->nl->donotflush_dbjnl);
	assert(!csa->hold_onto_crit || csa->now_crit);
	assert(0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1));
	if (!(was_crit = csa->now_crit))	/* Caution: assignment */
		grab_crit(gv_cur_region);
	/* jnl_enabled is an overloaded variable. It is TRUE only if JNL_ENABLED(csd) is TRUE
	 * and if the journal file has been opened in shared memory. If the journal file hasn't
	 * been opened in shared memory, we needn't (and shouldn't) do any journal file activity.
	 */
	jnl_enabled = (JNL_ENABLED(csd) && (0 != cnl->jnl_file.u.inode));
	jpc = csa->jnl;
	if (jnl_enabled)
	{
		jb = jpc->jnl_buff;
		/* Assert that we never flush the cache in the midst of a database commit. The only exception is MUPIP RUNDOWN */
		assert((csa->ti->curr_tn == csa->ti->early_tn) || in_mu_rndwn_file);
		if (!jgbl.dont_reset_gbl_jrec_time)
			SET_GBL_JREC_TIME;	/* needed before jnl_ensure_open */
		/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time (if needed) to maintain time order of jnl
		 * records. This needs to be done BEFORE the jnl_ensure_open as that could write journal records
		 * (if it decides to switch to a new journal file)
		 */
		ADJUST_GBL_JREC_TIME(jgbl, jb);
		assert(csa == cs_addrs);	/* for jnl_ensure_open */
		jnl_status = jnl_ensure_open();
		WBTEST_ASSIGN_ONLY(WBTEST_WCS_FLU_FAIL, jnl_status, ERR_JNLFILOPN);
		if (SS_NORMAL != jnl_status)
		{
			assert(ERR_JNLFILOPN == jnl_status);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
			if (JNL_ENABLED(csd))
			{	/* If journaling is still enabled, but we failed to open the journal file,
				 * we don't want to continue processing.
				 */
				REL_CRIT_BEFORE_RETURN;
				return FALSE;
			}
			jnl_enabled = FALSE;
		}
	}
	if (jnl_enabled)
	{
		assert(SS_NORMAL == jnl_status);
		if (return_early = (speedup_nobefore && !csd->jnl_before_image))
		{	/* Finish easiest option first. This database has NOBEFORE image journaling and caller has asked for
			 * processing to be speeded up in that case. Write only an epoch record, dont do heavyweight flush or fsync
			 * of db.This will avoid bunching of IO at the epoch time like is the case with before-image journaling
			 * where this is currently necessary for correctness. But for nobefore, there is no need to do this since
			 * no backward recovery will be performed. Note that if db has journaling disabled OR enabled with before-
			 * image journaling, we skip this portion of code and follow through to the rest of wcs_flu as if
			 * WCSFLU_SPEEDUP_NOBEFORE was not specified.
			 */
			assert(!jgbl.mur_extract); /* Dont know of a case where journal extract calls us with skip_db_flush set */
			assert(write_epoch);
			assert(flush_hdr);
			/* For Recovery/Rollback logic (even in case of NOBEFORE image journaling) to work correctly, the TN values
			 * in the file header - jnl_eovtn and curr_tn - should be greater than eov_tn in the journal file header.
			 * Note: eov_tn in the journal file header is the TN of the penultimate EPOCH and so should always be <=
			 * current database transaction number. If this relation is not maintained by GT.M, Rollback/Recovery logic
			 * can issue JNLDBTNNOMATCH error. To avoid this situation, flush and sync the DB file header.
			 */
			fileheader_sync(gv_cur_region);
			assert(NULL != jpc);
			if (0 == jpc->pini_addr)
				jnl_put_jrt_pini(csa);
			JNL_WRITE_EPOCH_REC(csa, cnl, clean_dbsync);
		}
		fsync_dskaddr = jb->fsync_dskaddr;	/* take a local copy as it could change concurrently */
		if (fsync_dskaddr != jb->freeaddr)
		{
			assert(fsync_dskaddr <= jb->dskaddr);
			if (SS_NORMAL != (jnl_status = jnl_flush(gv_cur_region)))
			{
				assert(NOJNL == jpc->channel); /* jnl file lost */
				REL_CRIT_BEFORE_RETURN;
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd), ERR_TEXT, 2,
					RTS_ERROR_TEXT("Error with journal flush during wcs_flu1"), jnl_status);
				return FALSE;
			}
			assert(jb->freeaddr == jb->dskaddr);
			jnl_fsync(gv_cur_region, jb->dskaddr);
			assert(jb->fsync_dskaddr == jb->dskaddr);
		}
		if (return_early)
		{
			REL_CRIT_BEFORE_RETURN;
			return TRUE;
		}
	}
	BG_TRACE_ANY(csa, total_buffer_flush);
	INCR_GVSTATS_COUNTER(csa, cnl, n_db_flush, 1);
	cnl->wcsflu_pid = process_id;
	if (dba_mm == csd->acc_meth)
	{
		if (WBTEST_ENABLED(WBTEST_WCS_FLU_FAIL)
			|| ((csd->freeze || flush_msync) && (csa->ti->last_mm_sync != csa->ti->curr_tn)))
		{
			if (!(WBTEST_ENABLED(WBTEST_WCS_FLU_FAIL))
				&& (0 == MSYNC((caddr_t)(MM_BASE_ADDR(csa)), (caddr_t)csa->db_addrs[1])))
			{	/* Save when did last full sync */
				csa->ti->last_mm_sync = csa->ti->curr_tn;
			} else
			{
				REL_CRIT_BEFORE_RETURN;
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), ERR_TEXT, 2,
					RTS_ERROR_TEXT("Error during file msync during flush"));
				return FALSE;
			}
		}
	}
	if (dba_mm != csd->acc_meth)
	{	/* If not mupip rundown, wait for ALL active phase2 commits to complete first.
		 * In case of mupip rundown, we know no one else is accessing shared memory so no point waiting.
		 */
		assert(!in_mu_rndwn_file || (0 == cnl->wcs_phase2_commit_pidcnt));
		if (WBTEST_ENABLED(WBTEST_WCS_FLU_FAIL) || (cnl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(csa, NULL)))
		{
			assert((WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number) /* see wcs_phase2_commit_wait.c */
				|| (WBTEST_WCS_FLU_FAIL == gtm_white_box_test_case_number));
			REL_CRIT_BEFORE_RETURN;
			return FALSE;	/* We expect the caller to trigger cache-recovery which will fix this counter */
		}
		/* Now that all concurrent commits are complete, wait for these dirty buffers to be flushed to disk.
		 * Note that calling wcs_wtstart just once assumes that if we ask it to flush all the buffers, it will.
		 * This may not be true in case of twins. But this is Unix. So not an issue.
		 */
		wtstart_errno = wcs_wtstart(gv_cur_region, csd->n_bts);		/* Flush it all */
		/* At this point the cache should have been flushed except if some other process is in wcs_wtstart waiting
		 * to flush the dirty buffer that it has already removed from the active queue. Wait for it to finish.
		 */
		fix_in_wtstart = FALSE;		/* set to TRUE by the following macro if we needed to correct cnl->in_wtstart */
		WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart, was_crit);
		/* Ideally at this point, the cache should have been flushed. But there is a possibility that the other
		 *   process in wcs_wtstart which had already removed the dirty buffer from the active queue found (because
		 *   csr->jnl_addr > jb->dskaddr) that it needs to be reinserted and placed it back in the active queue.
		 *   In this case, issue another wcs_wtstart to flush the cache. Even if a concurrent writer picks up an
		 *   entry, he should be able to write it out since the journal is already flushed.
		 * The check for whether the cache has been flushed is two-pronged. One via "wcs_active_lvl" and the other
		 *   via the active queue head. Ideally, both are interdependent and checking on "wcs_active_lvl" should be
		 *   enough, but we don't want to take a risk in PRO (in case wcs_active_lvl is incorrect).
		 */
		crq = &csa->acc_meth.bg.cache_state->cacheq_active;
		assert(((0 <= cnl->wcs_active_lvl) && (cnl->wcs_active_lvl || 0 == crq->fl)) || (ENOSPC == wtstart_errno));
#		ifdef DEBUG
		if (in_commit)
			GTM_WHITE_BOX_TEST(WBTEST_WCS_FLU_IOERR, cnl->wcs_active_lvl, 1);
		GTM_WHITE_BOX_TEST(WBTEST_ANTIFREEZE_OUTOFSPACE, cnl->wcs_active_lvl, 1);
#		endif
		if (cnl->wcs_active_lvl || crq->fl)
		{
			wtstart_errno = wcs_wtstart(gv_cur_region, csd->n_bts);		/* Flush it all */
			WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart, was_crit);
#			ifdef DEBUG
			if (in_commit)
			{
				GTM_WHITE_BOX_TEST(WBTEST_WCS_FLU_IOERR, cnl->wcs_active_lvl, 1);
				GTM_WHITE_BOX_TEST(WBTEST_WCS_FLU_IOERR, wtstart_errno, ENOENT);
			}
			if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_OUTOFSPACE == gtm_white_box_test_case_number))
			{
				cnl->wcs_active_lvl = 1;
				wtstart_errno = ENOSPC;
			}
#			endif
			if (cnl->wcs_active_lvl || crq->fl)		/* give allowance in PRO */
			{
				if (ENOSPC == wtstart_errno)
				{	/* wait for csd->wait_disk_space seconds, and give up if still not successful */
					to_wait = csd->wait_disk_space;
					to_msg = (to_wait / 8) ? (to_wait / 8) : 1; /* send message 8 times */
					while ((0 < to_wait) && (ENOSPC == wtstart_errno))
					{
						if ((to_wait == csd->wait_disk_space)
						    || (0 == to_wait % to_msg))
						{
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								 process_id, to_wait, DB_LEN_STR(gv_cur_region), wtstart_errno);
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								   process_id, to_wait, DB_LEN_STR(gv_cur_region), wtstart_errno);
						}
						hiber_start(1000);
						to_wait--;
						wtstart_errno = wcs_wtstart(gv_cur_region, csd->n_bts);
						if (0 == crq->fl)
							break;
					}
					if ((to_wait <= 0) && (cnl->wcs_active_lvl || crq->fl))
					{	/* not enough space became available after the wait */
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_OUTOFSPACE, 3,
								DB_LEN_STR(gv_cur_region), process_id);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_OUTOFSPACE, 3,
								DB_LEN_STR(gv_cur_region), process_id);
					}
				} else
				{	/* There are four different cases we know of currently when this is possible:
					 * (a) If a process encountered an error in the midst of committing in phase2 and
					 * secshr_db_clnup completed the commit for it and set wc_blocked to TRUE (even though
					 * it was OUT of crit) causing the wcs_wtstart calls done above to do nothing.
					 * (b) If a process performing multi-region TP transaction encountered an error in
					 * phase1 of the commit, but at least one of the participating regions have completed
					 * the phase1 and released crit, secshr_db_clnup will set wc_blocked on all the regions
					 * (including those that will be OUTSIDE crit) that participated in the commit. Hence,
					 * like (a), wcs_wtstart calls done above will return immediately.
					 * But phase1 and phase2 commit errors are currently enabled only through white-box testing.
					 * (c) If a test does crash shutdown (kill -9) that hit the process in the middle of
					 * wcs_wtstart which means the writes did not complete successfully.
					 * (d) If WBTEST_WCS_FLU_IOERR/WBTEST_WCS_WTSTART_IOERR white box test case is set that
					 * forces wcs_wtstart invocations to end up with I/O errors.
					 */
					assert((WBTEST_BG_UPDATE_PHASE2FAIL == gtm_white_box_test_case_number)
						|| (WBTEST_BG_UPDATE_BTPUTNULL == gtm_white_box_test_case_number)
						|| (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number)
						|| (WBTEST_WCS_FLU_IOERR == gtm_white_box_test_case_number)
						|| (WBTEST_WCS_WTSTART_IOERR == gtm_white_box_test_case_number)
						|| (WBTEST_ANTIFREEZE_DBDANGER == gtm_white_box_test_case_number)
					        || (WBTEST_ANTIFREEZE_JNLCLOSE == gtm_white_box_test_case_number));
					if (0 == wtstart_errno)
					{
						SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wcb_wcs_flu1);
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
								LEN_AND_LIT("wcb_wcs_flu1"), process_id, &csa->ti->curr_tn,
								DB_LEN_STR(gv_cur_region));
					} else
					{	/* Encountered I/O error. Transfer control to error trap */
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBIOERR, 4, REG_LEN_STR(gv_cur_region),
								DB_LEN_STR(gv_cur_region), wtstart_errno);
					}
					if (in_commit)
					{	/* We should NOT be invoking wcs_recover as otherwise the callers (t_end or tp_tend)
						 * will get confused (see explanation above where variable "in_commit" gets set).
						 */
						assert(was_crit);	/* so dont need to rel_crit */
						cnl->wcsflu_pid = 0;
						return FALSE;
					}
					assert(!jnl_enabled || jb->fsync_dskaddr == jb->freeaddr);
					if (0 == wtstart_errno)
						wcs_recover(gv_cur_region);
					if (jnl_enabled)
					{
						fsync_dskaddr = jb->fsync_dskaddr;
							/* take a local copy as it could change concurrently */
						if (fsync_dskaddr != jb->freeaddr)
						{	/* an INCTN record should have been written above */
							assert(fsync_dskaddr <= jb->dskaddr);
							assert((jb->freeaddr - fsync_dskaddr) >= INCTN_RECLEN);
							/* above assert has a >= instead of == due to possible
							 * ALIGN record in between */
							if (SS_NORMAL != (jnl_status = jnl_flush(gv_cur_region)))
							{
								assert(NOJNL == jpc->channel); /* jnl file lost */
								REL_CRIT_BEFORE_RETURN;
								send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2,
									JNL_LEN_STR(csd), ERR_TEXT, 2,
									RTS_ERROR_TEXT("Error with journal flush during wcs_flu2"),
									jnl_status);
								return FALSE;
							}
							assert(jb->freeaddr == jb->dskaddr);
							jnl_fsync(gv_cur_region, jb->dskaddr);
							/* Use jb->fsync_dskaddr (instead of "fsync_dskaddr") below as the
							 * shared memory copy is more uptodate (could have been updated by
							 * "jnl_fsync" call above).
							 */
							assert(jb->fsync_dskaddr == jb->dskaddr);
						}
					}
					wcs_wtstart(gv_cur_region, csd->n_bts);		/* Flush it all */
					WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart, was_crit);
					if (cnl->wcs_active_lvl || crq->fl)
					{
						REL_CRIT_BEFORE_RETURN;
						GTMASSERT;
					}
				}
			}
		}
	}
	if (flush_hdr)
		fileheader_sync(gv_cur_region);
	if (jnl_enabled && write_epoch)
	{	/* If need to write an epoch,
		 *	(1) get hold of the jnl io_in_prog lock.
		 *	(2) set need_db_fsync to TRUE in the journal buffer.
		 *	(3) release the jnl io_in_prog lock.
		 *	(4) write an epoch record in the journal buffer.
		 * The next call to jnl_qio_start will do the fsync of the db before doing any jnl qio.
		 * The basic requirement is that we shouldn't write the epoch out until we have synced the database.
		 */
		assert(jb->fsync_dskaddr == jb->freeaddr);
		/* If jb->need_db_fsync is TRUE at this point of time, it means we already have a db_fsync waiting
		 * to happen. This means the epoch due to the earlier need_db_fsync hasn't yet been written out to
		 * the journal file. But that means we haven't yet flushed the journal buffer which leads to a
		 * contradiction. (since we have called jnl_flush earlier in this routine and also assert to the
		 * effect jb->fsync_dskaddr == jb->freeaddr a few lines above).
		 */
		assert(!jb->need_db_fsync);
		for (lcnt = 1; FALSE == (GET_SWAPLOCK(&jb->io_in_prog_latch)); lcnt++)
		{
			if (MAXJNLQIOLOCKWAIT < lcnt)	/* tried too long */
			{
				GET_C_STACK_MULTIPLE_PIDS("MAXJNLQIOLOCKWAIT", cnl->wtstart_pid, MAX_WTSTART_PID_SLOTS, 1);
				assert(FALSE);
				REL_CRIT_BEFORE_RETURN;
				GTMASSERT;
			}
			wcs_sleep(SLEEP_JNLQIOLOCKWAIT);	/* since it is a short lock, sleep the minimum */

			if ((MAXJNLQIOLOCKWAIT / 2 == lcnt) || (MAXJNLQIOLOCKWAIT == lcnt))
				performCASLatchCheck(&jb->io_in_prog_latch, TRUE);
		}
		if (csd->jnl_before_image)
			jb->need_db_fsync = TRUE;	/* for comments on need_db_fsync, see jnl_output_sp.c */
		/* else the journal files do not support before images and hence can only be used for forward recovery. So skip
		 * fsync of the database (jb->need_db_fsync = FALSE) because we don't care if the on-disk db is up-to-date or not.
		 */
		RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		assert(!(JNL_FILE_SWITCHED(jpc)));
		assert(jgbl.gbl_jrec_time);
		if (!jgbl.mur_extract)
		{
			if (0 == jpc->pini_addr)
				jnl_put_jrt_pini(csa);
			JNL_WRITE_EPOCH_REC(csa, cnl, clean_dbsync);
		}
	}
	cnl->last_wcsflu_tn = csa->ti->curr_tn;	/* record when last successful wcs_flu occurred */
	REL_CRIT_BEFORE_RETURN;
	/* sync the epoch record in the journal if needed. */
	if (jnl_enabled && write_epoch && sync_epoch && (csa->ti->curr_tn == csa->ti->early_tn))
	{	/* Note that if we are in the midst of committing and came here through a bizarre
		 * stack trace (like wcs_get_space etc.) we want to defer syncing to when we go out of crit.
		 * Note that we are guaranteed to come back to wcs_wtstart since we are currently in commit-phase
		 * and will dirty atleast one block as part of the commit for a wtstart timer to be triggered.
		 */
		jnl_wait(gv_cur_region);
	}
	if (need_db_fsync && JNL_ALLOWED(csd))
	{
		if (dba_mm != csd->acc_meth)
		{
			DB_FSYNC(gv_cur_region, udi, csa, db_fsync_in_prog, save_errno);
			if (0 != save_errno)
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(gv_cur_region), save_errno);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(gv_cur_region), save_errno);
				assert(FALSE);	/* should not come here as the rts_error above should not return */
				return FALSE;
			}
		}
	}
	return TRUE;
}
