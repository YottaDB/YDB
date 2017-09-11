/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "wcs_backoff.h"
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
#include "wcs_wt.h"

GBLREF	bool			in_backup;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	uint4			process_id;
GBLREF	volatile int4		db_fsync_in_prog;	/* for DB_FSYNC macro usage */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	bool			in_mupip_freeze;
#ifdef DEBUG
GBLREF	boolean_t		in_mu_rndwn_file;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		mupip_jnl_recover;
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
MBSTART {										\
	jnl_write_epoch_rec(CSA);							\
	/* Note: Cannot easily use ? : syntax below as INCR_GVSTATS_COUNTER macro	\
	 * is not an arithmetic expression but a sequence of statements.		\
	 */										\
	if (!CLEAN_DBSYNC)								\
	{										\
		INCR_GVSTATS_COUNTER(CSA, CNL, n_jrec_epoch_regular, 1);		\
	} else										\
		INCR_GVSTATS_COUNTER(CSA, CNL, n_jrec_epoch_idle, 1);			\
} MBEND

#define	WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(FIX_IN_WTSTART, WAS_CRIT, REG, CSA, CNL)					\
MBSTART {														\
	unsigned int		lcnt;											\
        struct shmid_ds         shm_buf;										\
	int			save_errno;										\
															\
	GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, (CNL->in_wtstart), 1);						\
	if (WRITERS_ACTIVE(CNL))											\
	{														\
		DEBUG_ONLY(int4	in_wtstart;) 		/* temporary for debugging purposes */				\
		DEBUG_ONLY(int4	intent_wtstart;) 	/* temporary for debugging purposes */				\
															\
		assert(CSA->now_crit);											\
		SIGNAL_WRITERS_TO_STOP(CNL);		/* to stop all active writers */				\
		lcnt = 0;												\
		do													\
		{													\
			DEBUG_ONLY(in_wtstart = CNL->in_wtstart;)							\
			DEBUG_ONLY(intent_wtstart = CNL->intent_wtstart;)						\
			GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, lcnt, (MAXGETSPACEWAIT * 2) - 1);		\
			GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, CNL->wtstart_pid[0], process_id);		\
			if (MAXGETSPACEWAIT DEBUG_ONLY( * 2) == ++lcnt)							\
			{	/* We have noticed the below assert to fail occasionally on some platforms (mostly	\
				 * AIX and Linux). We suspect it is because of waiting for another writer that is 	\
				 * in jnl_fsync (as part of flushing a global buffer) which takes more than a minute	\
				 * to finish. To avoid false failures (where the other writer finishes its job in	\
				 * a little over a minute) we wait for twice the time in the debug version.		\
				 */											\
				GET_C_STACK_MULTIPLE_PIDS("WRITERSTUCK", CNL->wtstart_pid, MAX_WTSTART_PID_SLOTS, 1);	\
				assert((gtm_white_box_test_case_enabled)						\
					&& ((WBTEST_BUFOWNERSTUCK_STACK == gtm_white_box_test_case_number)		\
						|| (WBTEST_SLEEP_IN_WCS_WTSTART == gtm_white_box_test_case_number)));	\
				CNL->wcsflu_pid = 0;									\
				SIGNAL_WRITERS_TO_RESUME(CNL);								\
				if (!WAS_CRIT)										\
					rel_crit(REG);									\
				/* Disable white box testing after the first time the					\
				WBTEST_BUFOWNERSTUCK_STACK mechanism has kicked in. This is because as			\
				part of the exit handling process, the control once agin comes to wcs_flu		\
				and at that time we do not want the WBTEST_BUFOWNERSTUCK_STACK white box		\
				mechanism to kick in.*/									\
				GTM_WHITE_BOX_TEST(WBTEST_BUFOWNERSTUCK_STACK, gtm_white_box_test_case_enabled, FALSE);	\
				send_msg_csa(CSA_ARG(CSA) VARLSTCNT(5) ERR_WRITERSTUCK, 3, CNL->in_wtstart,		\
						DB_LEN_STR(REG));							\
				return FALSE;										\
			}												\
			if (-1 == shmctl(udi->shmid, IPC_STAT, &shm_buf))						\
			{												\
				save_errno = errno;									\
				if (1 == lcnt)										\
				{											\
					send_msg_csa(CSA_ARG(CSA) VARLSTCNT(4) ERR_DBFILERR, 2,				\
							DB_LEN_STR(REG));						\
					send_msg_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_SYSCALL, 5,				\
							RTS_ERROR_LITERAL("shmctl()"), CALLFROM, save_errno);		\
				} 											\
			} else if (1 == shm_buf.shm_nattch)								\
			{												\
				assert((FALSE == CSA->in_wtstart) && (0 <= CNL->in_wtstart));				\
				CNL->in_wtstart = 0;	/* fix improper value of in_wtstart if you are standalone */	\
				FIX_IN_WTSTART = TRUE;									\
				CNL->intent_wtstart = 0;/* fix improper value of intent_wtstart if standalone */	\
			} else												\
				wcs_sleep(lcnt);		/* wait for any in wcs_wtstart to finish */		\
		} while (WRITERS_ACTIVE(CNL));										\
		SIGNAL_WRITERS_TO_RESUME(CNL);										\
	}														\
} MBEND

#define REL_CRIT_BEFORE_RETURN(CNL, REG)	\
MBSTART {					\
	CNL->doing_epoch = FALSE;		\
	CNL->wcsflu_pid = 0;			\
	if (!was_crit)				\
		rel_crit(REG);			\
} MBEND

/* The below macro returns TRUE if there is some cache-record is likely still dirty in
 *	a) active queue     : (CNL->wcs_active_lvl || CRQ->fl) check OR
 *	b) wip queue        : (CRWIPQ->fl)                     check OR
 *	c) in neither queue : (N_BTS != CNL->wc_in_free)       check
 */
#define	FLUSH_NOT_COMPLETE(CNL, CRQ, CRWIPQ, N_BTS) (CNL->wcs_active_lvl || CRQ->fl || CRWIPQ->fl || (N_BTS != CNL->wc_in_free))

/* Sets RET to FALSE if the caller needs to do a "return FALSE" after macro returns. Sets RET to TRUE otherwise. */
#define	CLEAR_WIP_QUEUE_IF_NEEDED(ASYNCIO, WTSTART_OR_WTFINI_ERRNO, CNL, CRWIPQ, REG, RET)		\
MBSTART {												\
	int	wtfini_errno;										\
													\
	RET = TRUE;											\
	if (ASYNCIO)											\
	{												\
		assert(ENOSPC != WTSTART_OR_WTFINI_ERRNO);						\
		WAIT_FOR_WIP_QUEUE_TO_CLEAR(CNL, CRWIPQ, ((cache_rec_ptr_t) NULL), REG, wtfini_errno);	\
		if (wtfini_errno)									\
		{											\
			if (ENOSPC != wtfini_errno)							\
			{										\
				assert(FALSE);								\
				REL_CRIT_BEFORE_RETURN(CNL, REG);					\
				RET = FALSE;								\
			} else										\
			{										\
				assert(!WTSTART_OR_WTFINI_ERRNO);					\
				if (!WTSTART_OR_WTFINI_ERRNO)						\
					WTSTART_OR_WTFINI_ERRNO = wtfini_errno;				\
			}										\
		}											\
	}												\
} MBEND

boolean_t wcs_flu(uint4 options)
{
	boolean_t		was_crit, ret;
	boolean_t		fix_in_wtstart, flush_hdr, jnl_enabled, sync_epoch, write_epoch, need_db_fsync, in_commit;
	boolean_t		flush_msync, speedup_nobefore, clean_dbsync, return_early, epoch_already_current, asyncio;
	boolean_t		force_epoch;
	boolean_t		latch_salvaged;
	unsigned int		lcnt, pass;
	int			n_bts, save_errno, wtstart_or_wtfini_errno;
	jnl_buffer_ptr_t	jb;
	jnl_private_control	*jpc;
	uint4			jnl_status, to_wait, to_msg;
	unix_db_info    	*udi;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	file_control		*fc;
	cache_que_head_ptr_t	crq, crwipq;
	uint4			fsync_dskaddr;
	int4			rc;
	gd_region		*reg;
#	ifdef DEBUG
	int			wcs_wip_lvl, wcs_active_lvl, wc_in_free; /* copy of cnl noted down for debugging purposes */
#	endif

	jnl_status = 0;
	flush_hdr = options & WCSFLU_FLUSH_HDR;
	write_epoch = options & WCSFLU_WRITE_EPOCH;
	sync_epoch = options & WCSFLU_SYNC_EPOCH;
	need_db_fsync = options & WCSFLU_FSYNC_DB;
	flush_msync = options & WCSFLU_MSYNC_DB;
	speedup_nobefore = options & WCSFLU_SPEEDUP_NOBEFORE;
	clean_dbsync = options & WCSFLU_CLEAN_DBSYNC;
	force_epoch = options & WCSFLU_FORCE_EPOCH;
	/* WCSFLU_IN_COMMIT bit is set if caller is "t_end" or "tp_tend" or a few other functions (currently only "view_dbop").
	 * This flag is an indication that we should NOT invoke "wcs_recover" if we enter this function while already holding
	 * crit (because the caller will otherwise get confused by the persistent effects of "wcs_recover")). Instead we should
	 * return the error as such so they can trigger appropriate error handling. This is necessary because t_end and tp_tend
	 * could have pinned one or more cache-records (cr->in_cw_set non-zero) BEFORE invoking wcs_flu. And code AFTER the
	 * wcs_flu in them relies on the fact that those cache records stay pinned. If wcs_flu invokes wcs_recover, it will
	 * reset cr->in_cw_set to 0 for ALL cache-records so code AFTER the wcs_flu in the caller will fail because no buffer
	 * is pinned at that point. As for "view_dbop", the reason is captured in a comment in the caller.
	 */
	in_commit = options & WCSFLU_IN_COMMIT;
	reg = gv_cur_region;
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	/* We do not want to do costly WCSFLU_SYNC_EPOCH inside crit. Only exception is if caller holds crit for a lot longer
	 * than the current operation (e.g. DSE CRIT SEIZE etc.). csa->hold_onto_crit is TRUE in that case. Assert that.
	 */
	assert(!sync_epoch || csa->hold_onto_crit || !csa->now_crit);
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
	jpc = csa->jnl;
	if (!(was_crit = csa->now_crit))	/* Caution: assignment */
	{
		DO_DB_FSYNC_OUT_OF_CRIT_IF_NEEDED(reg, csa, jpc, jpc->jnl_buff);
		grab_crit_encr_cycle_sync(reg);
		/* If it is safe to invoke "wcs_recover" (indicated by the in_commit variable being 0), do that right away
		 * to fix any dead phase2 commits if needed.
		 */
		if (!in_commit && cnl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(csa, NULL))
		{	/* Since phase2-commit-wait failed, set wc_blocked to TRUE if not already set.
			 * Since we hold crit and know it is safe to invoke "wcs_recover", do invoke it.
			 * But it is possible it returns right away (e.g. "is_src_server" is TRUE). But that
			 * is okay since "cnl->wc_blocked" would stay set so someone else who gets crit
			 * (other than the source server) would do the "wcs_recover" call.
			 */
			if (!cnl->wc_blocked)
			{
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wcs_flu0);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
						LEN_AND_LIT("wcb_wcs_flu0"), process_id, &csa->ti->curr_tn,
						DB_LEN_STR(reg));
			}
			wcs_recover(reg);
			assert(!cnl->wcs_phase2_commit_pidcnt || is_src_server); /* source server does not do "wcs_recover" */
			if (cnl->wcs_phase2_commit_pidcnt)
			{
				REL_CRIT_BEFORE_RETURN(cnl, reg);
				return FALSE;
			}
		}
	}
	if (!FREEZE_LATCH_HELD(csa))
		WAIT_FOR_REGION_TO_UNCHILL(csa, csd);
	/* jnl_enabled is an overloaded variable. It is TRUE only if JNL_ENABLED(csd) is TRUE
	 * and if the journal file has been opened in shared memory. If the journal file hasn't
	 * been opened in shared memory, we needn't (and shouldn't) do any journal file activity.
	 */
	jnl_enabled = (JNL_ENABLED(csd) && (0 != cnl->jnl_file.u.inode));
	if (jnl_enabled)
	{
		jb = jpc->jnl_buff;
		/* If we are trying to flush a completed journal file, make sure there is nothing else to do and return. */
		if (jb->last_eof_written)
		{
			assert(jb->fsync_dskaddr == jb->freeaddr);
			assert(jb->rsrv_freeaddr == jb->freeaddr);
			assert((dba_bg != csd->acc_meth) || !csd->jnl_before_image
				|| (!cnl->wcs_active_lvl && !csa->acc_meth.bg.cache_state->cacheq_active.fl));
			REL_CRIT_BEFORE_RETURN(cnl, reg);
			return TRUE;
		}
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
		jnl_status = jnl_ensure_open(reg, csa);
		WBTEST_ASSIGN_ONLY(WBTEST_WCS_FLU_FAIL, jnl_status, ERR_JNLFILOPN);
		if (SS_NORMAL != jnl_status)
		{
			assert(ERR_JNLFILOPN == jnl_status);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
			if (JNL_ENABLED(csd))
			{	/* If journaling is still enabled, but we failed to open the journal file,
				 * we don't want to continue processing.
				 */
				REL_CRIT_BEFORE_RETURN(cnl, reg);
				return FALSE;
			}
			jnl_enabled = FALSE;
		}
	}
	if (jnl_enabled)
	{
		assert(SS_NORMAL == jnl_status);
		cnl->doing_epoch = sync_epoch || write_epoch;
		epoch_already_current = (!force_epoch && (jb->post_epoch_freeaddr == jb->rsrv_freeaddr));
		if (return_early = (speedup_nobefore && !csd->jnl_before_image))
		{	/* Finish easiest option first. This database has NOBEFORE image journaling and caller has asked for
			 * processing to be speeded up in that case. Write only an epoch record, don't do heavyweight flush or fsync
			 * of db.This will avoid bunching of IO at the epoch time like is the case with before-image journaling
			 * where this is currently necessary for correctness. But for nobefore, there is no need to do this since
			 * no backward recovery will be performed. Note that if db has journaling disabled OR enabled with before-
			 * image journaling, we skip this portion of code and follow through to the rest of wcs_flu as if
			 * WCSFLU_SPEEDUP_NOBEFORE was not specified.
			 */
			assert(!jgbl.mur_extract); /* Don't know of a case where journal extract calls us with skip_db_flush set */
			assert(write_epoch);
			assert(flush_hdr);
			/* For Recovery/Rollback logic (even in case of NOBEFORE image journaling) to work correctly, the TN values
			 * in the file header - jnl_eovtn and curr_tn - should be greater than eov_tn in the journal file header.
			 * Note: eov_tn in the journal file header is the TN of the penultimate EPOCH and so should always be <=
			 * current database transaction number. If this relation is not maintained by GT.M, Rollback/Recovery logic
			 * can issue JNLDBTNNOMATCH error. To avoid this situation, flush and sync the DB file header.
			 */
			fileheader_sync(reg);
			assert(NULL != jpc);
			if (!jgbl.mur_extract && !epoch_already_current)
			{
				if (0 == jpc->pini_addr)
					jnl_write_pini(csa);
				JNL_WRITE_EPOCH_REC(csa, cnl, clean_dbsync);
			} else if (epoch_already_current)
				jb->next_epoch_time = MAXUINT4;
		}
		fsync_dskaddr = jb->fsync_dskaddr;	/* take a local copy as it could change concurrently */
		if (fsync_dskaddr != jb->rsrv_freeaddr)
		{
			assert((fsync_dskaddr <= jb->dskaddr) || WBTEST_ENABLED(WBTEST_JNL_FILE_LOST_DSKADDR));
			if (SS_NORMAL != (jnl_status = jnl_flush(reg)))
			{
				assert(NOJNL == jpc->channel); /* jnl file lost */
				REL_CRIT_BEFORE_RETURN(cnl, reg);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd), ERR_TEXT, 2,
					RTS_ERROR_TEXT("Error with journal flush during wcs_flu1"), jnl_status);
				return FALSE;
			}
#			ifdef DEBUG
			if (!gtm_white_box_test_case_enabled || (WBTEST_JNL_FILE_LOST_DSKADDR != gtm_white_box_test_case_number))
			{
				assert(jb->rsrv_freeaddr == jb->dskaddr);
				assert(jb->rsrv_freeaddr == jb->freeaddr);
			}
#			endif
			jnl_fsync(reg, jb->dskaddr);
			assert(jb->fsync_dskaddr == jb->dskaddr);
		}
		if (return_early)
		{
			REL_CRIT_BEFORE_RETURN(cnl, reg);
			return TRUE;
		}
	}
	BG_TRACE_ANY(csa, total_buffer_flush);
	INCR_GVSTATS_COUNTER(csa, cnl, n_db_flush, 1);
	cnl->wcsflu_pid = process_id;
	if (dba_mm == csd->acc_meth)
	{
		if (WBTEST_ENABLED(WBTEST_WCS_FLU_FAIL)
			|| ((FROZEN(csd) || flush_msync) && (csa->ti->last_mm_sync != csa->ti->curr_tn)))
		{
			#ifdef _AIX
			GTM_DB_FSYNC(csa, udi->fd, rc);
			#else
			rc = MSYNC((caddr_t)(MM_BASE_ADDR(csa)), (caddr_t)csa->db_addrs[1]);
			#endif
			if (!(WBTEST_ENABLED(WBTEST_WCS_FLU_FAIL)) && (0 == rc))
			{	/* Save when did last full sync */
				csa->ti->last_mm_sync = csa->ti->curr_tn;
			} else
			{
				REL_CRIT_BEFORE_RETURN(cnl, reg);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
					RTS_ERROR_TEXT("Error during file msync during flush"));
				return FALSE;
			}
		}
	} else
	{	/* If not mupip rundown, wait for ALL active phase2 commits to complete first.
		 * In case of mupip rundown, we know no one else is accessing shared memory so no point waiting.
		 */
		asyncio = csd->asyncio;
		assert(!in_mu_rndwn_file || (0 == cnl->wcs_phase2_commit_pidcnt));
		/* We already did "wcs_phase2_commit_wait" for !was_crit && !in_commit case. Assert that below. */
		assert(was_crit || in_commit || !cnl->wcs_phase2_commit_pidcnt);
		if (WBTEST_ENABLED(WBTEST_WCS_FLU_FAIL) || (cnl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(csa, NULL)))
		{
			assert((WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number) /* see wcs_phase2_commit_wait.c */
					|| (WBTEST_WCS_FLU_FAIL == gtm_white_box_test_case_number));
			REL_CRIT_BEFORE_RETURN(cnl, reg);
			return FALSE;	/* We expect the caller to trigger cache-recovery which will fix this counter */
		}
		/* Now that all concurrent commits are complete, wait for these dirty buffers to be flushed to disk.
		 * Note that calling wcs_wtstart just once assumes that if we ask it to flush all the buffers, it will.
		 * This may not be true in case of twins since "wcs_wtstart" has to wait for the twin link to be broken
		 * by "wcs_wtfini" before it can issue the write of the newer twin. We handle that case by calling
		 * "wcs_wtstart" again down below.
		 */
		WCS_OPS_TRACE(csa, process_id, wcs_ops_flu1, 0, 0, 0, 0, 0);
		n_bts = csd->n_bts;
		wtstart_or_wtfini_errno = wcs_wtstart(reg, n_bts, NULL, NULL);		/* Flush it all */
		/* At this point the cache should have been flushed except if some other process is in wcs_wtstart waiting
		 * to flush the dirty buffer that it has already removed from the active queue. Wait for it to finish.
		 */
		fix_in_wtstart = FALSE;		/* set to TRUE by the following macro if we needed to correct cnl->in_wtstart */
#		ifdef DEBUG
		wcs_wip_lvl = cnl->wcs_wip_lvl;
		wcs_active_lvl = cnl->wcs_active_lvl;
		wc_in_free = cnl->wc_in_free;
#		endif
		crwipq = &csa->acc_meth.bg.cache_state->cacheq_wip;
		assert(asyncio || !crwipq->fl);
		WCS_OPS_TRACE(csa, process_id, wcs_ops_flu2, 0, 0, 0, 0, 0);
		WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart, was_crit, reg, csa, cnl);
		CLEAR_WIP_QUEUE_IF_NEEDED(asyncio, wtstart_or_wtfini_errno, cnl, crwipq, reg, ret);
		if (!ret)
		{	/* We expect caller to trigger cache-recovery which will fix the wip queue */
			return FALSE;
		}
		WCS_OPS_TRACE(csa, process_id, wcs_ops_flu3, 0, 0, 0, 0, 0);
		crq = &csa->acc_meth.bg.cache_state->cacheq_active;
		/* At this point, we expect the cache to be flushed. Exceptions are
		 *	a) twinning : "wcs_wtstart" could be waiting for a twin to be broken by "wcs_wtfini"
		 *		OR "wcs_wtfini" could have reinserted a cr back in active queue because cr->epid
		 *		corresponded to a dead pid.
		 *	b) A concurrent writer (that was active before we did the "wcs_wtstart" above) had removed a
		 *		cache record from the active queue but could not flush it out
		 *		(e.g. cr->jnl_addr > jb->fsync_dskaddr) and so reinserted it back in the active queue.
		 *	c) ENOSPC errors
		 *	d) white-box cases which induce error codepaths.
		 * All above exceptions except (b) can be accurately characterized. As for (b), we tried capturing "writers_active"
		 *	just before the wcs_wtstart above but it is possible that when we noted "writes_active", there were
		 *	no concurrent writers but one started soon afterwards and before we did our "wcs_wtstart".
		 *	Because of this, we do not assert anything below for any of the above 4 exceptions.
		 */
#		ifdef DEBUG
		/* White-box code to exercise error codepaths */
		if (in_commit)
			GTM_WHITE_BOX_TEST(WBTEST_WCS_FLU_IOERR, cnl->wcs_active_lvl, 1);
		GTM_WHITE_BOX_TEST(WBTEST_ANTIFREEZE_OUTOFSPACE, cnl->wcs_active_lvl, 1);
#		endif
		if (FLUSH_NOT_COMPLETE(cnl, crq, crwipq, n_bts))
		{	/* Some cache-record is likely still dirty in either active queue or wip queue or in neither queue */
			wtstart_or_wtfini_errno = wcs_wtstart(reg, n_bts, NULL, NULL);		/* Flush it all */
			WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart, was_crit, reg, csa, cnl);
			WCS_OPS_TRACE(csa, process_id, wcs_ops_flu4, 0, 0, 0, 0, 0);
			CLEAR_WIP_QUEUE_IF_NEEDED(asyncio, wtstart_or_wtfini_errno, cnl, crwipq, reg, ret);
			if (!ret)
			{	/* We expect caller to trigger cache-recovery which will fix the wip queue */
				return FALSE;
			}
			WCS_OPS_TRACE(csa, process_id, wcs_ops_flu5, 0, 0, 0, 0, 0);
#			ifdef DEBUG
			if (in_commit)
			{
				GTM_WHITE_BOX_TEST(WBTEST_WCS_FLU_IOERR, cnl->wcs_active_lvl, 1);
				GTM_WHITE_BOX_TEST(WBTEST_WCS_FLU_IOERR, wtstart_or_wtfini_errno, ENOENT);
			}
			if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_OUTOFSPACE == gtm_white_box_test_case_number))
			{	/* Simulate an ENOSPC return from "wcs_wtstart" or "wcs_wtfini" (if asyncio is TRUE) */
				cnl->wcs_active_lvl = 1;
				wtstart_or_wtfini_errno = ENOSPC;
			}
#			endif
			if (FLUSH_NOT_COMPLETE(cnl, crq, crwipq, n_bts)) /* give allowance in PRO */
			{
				if (ENOSPC == wtstart_or_wtfini_errno)
				{	/* wait for at least csd->wait_disk_space seconds, and give up if still not successful */
					WCS_OPS_TRACE(csa, process_id, wcs_ops_flu6, 0, 0, 0, 0, 0);
					to_wait = csd->wait_disk_space;
					to_msg = (to_wait / 8) ? (to_wait / 8) : 1; /* send message 8 times */
					while ((0 < to_wait) && (ENOSPC == wtstart_or_wtfini_errno))
					{
						if ((to_wait == csd->wait_disk_space) || (0 == (to_wait % to_msg)))
						{
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								 process_id, to_wait, DB_LEN_STR(reg), wtstart_or_wtfini_errno);
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								   process_id, to_wait, DB_LEN_STR(reg), wtstart_or_wtfini_errno);
						}
						hiber_start(1000);
						to_wait--;
						wtstart_or_wtfini_errno = wcs_wtstart(reg, n_bts, NULL, NULL);	/* Flush it all */
						CLEAR_WIP_QUEUE_IF_NEEDED(asyncio, wtstart_or_wtfini_errno, cnl, crwipq, reg, ret);
						if (!ret)
						{	/* We expect caller to trigger cache-recovery which will fix wip queue */
							return FALSE;
						}
						if (!FLUSH_NOT_COMPLETE(cnl, crq, crwipq, n_bts))
							break;
					}
					if ((to_wait <= 0) && FLUSH_NOT_COMPLETE(cnl, crq, crwipq, n_bts))
					{	/* not enough space became available after the wait */
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_OUTOFSPACE, 3,
								DB_LEN_STR(reg), process_id);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_OUTOFSPACE, 3,
								DB_LEN_STR(reg), process_id);
					}
				} else
				{	/* There are different cases we know of currently when this is possible all of which
					 * we currently test with white-box test cases.
					 * (a) If a process encountered an error in the midst of committing in phase2 and
					 *    secshr_db_clnup completed the commit for it and set wc_blocked to TRUE (even though
					 *    it was OUT of crit) causing the wcs_wtstart calls done above to do nothing.
					 * (b) If a process performing multi-region TP transaction encountered an error in
					 *    phase1 of the commit, but at least one of the participating regions have completed
					 *    the phase1 and released crit, secshr_db_clnup will set wc_blocked on all the regions
					 *    (including those that will be OUTSIDE crit) that participated in the commit. Hence,
					 *    like (a), wcs_wtstart calls done above will return immediately. But phase1 and
					 *    phase2 commit errors are currently enabled only through white-box testing.
					 * (c) If a test does crash shutdown (kill -9) that hit the process in the middle of
					 *    wcs_wtstart which means the writes did not complete successfully.
					 * (d) If WBTEST_WCS_FLU_IOERR/WBTEST_WCS_WTSTART_IOERR white box test case is set that
					 *    forces wcs_wtstart invocations to end up with I/O errors.
					 */
					WCS_OPS_TRACE(csa, process_id, wcs_ops_flu7, 0, 0, 0, wtstart_or_wtfini_errno, 0);
					assert((WBTEST_BG_UPDATE_PHASE2FAIL == gtm_white_box_test_case_number)
						|| (WBTEST_BG_UPDATE_BTPUTNULL == gtm_white_box_test_case_number)
						|| (WBTEST_BG_UPDATE_DBCSHGET_INVALID == gtm_white_box_test_case_number)
						|| (WBTEST_BG_UPDATE_DBCSHGETN_INVALID == gtm_white_box_test_case_number)
						|| (WBTEST_BG_UPDATE_DBCSHGETN_INVALID2 == gtm_white_box_test_case_number)
						|| (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number)
						|| (WBTEST_WCS_FLU_IOERR == gtm_white_box_test_case_number)
						|| (WBTEST_WCS_WTSTART_IOERR == gtm_white_box_test_case_number)
						|| (WBTEST_ANTIFREEZE_JNLCLOSE == gtm_white_box_test_case_number)
						|| ((WBTEST_ANTIFREEZE_OUTOFSPACE == gtm_white_box_test_case_number) && asyncio));
					if (0 == wtstart_or_wtfini_errno)
					{
						SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wcb_wcs_flu1);
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
								LEN_AND_LIT("wcb_wcs_flu1"), process_id, &csa->ti->curr_tn,
								DB_LEN_STR(reg));
					} else
					{	/* Encountered I/O error. Transfer control to error trap */
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBIOERR, 4, REG_LEN_STR(reg),
								DB_LEN_STR(reg), wtstart_or_wtfini_errno);
						assert(FALSE);	/* control should not come back here */
					}
					if (in_commit)
					{	/* We should NOT be invoking wcs_recover as otherwise the callers (t_end or tp_tend)
						 * will get confused (see explanation above where variable "in_commit" gets set).
						 */
						assert(was_crit);	/* so don't need to rel_crit */
						cnl->doing_epoch = FALSE;
						cnl->wcsflu_pid = 0;
						return FALSE;
					}
					assert(!jnl_enabled || jb->fsync_dskaddr == jb->rsrv_freeaddr);
					assert(0 == wtstart_or_wtfini_errno);
					wcs_recover(reg);
					if (jnl_enabled)
					{
						fsync_dskaddr = jb->fsync_dskaddr;
							/* take a local copy as it could change concurrently */
						if (fsync_dskaddr != jb->rsrv_freeaddr)
						{	/* an INCTN record should have been written above */
							assert(fsync_dskaddr <= jb->dskaddr);
							assert((jb->rsrv_freeaddr - fsync_dskaddr) >= INCTN_RECLEN);
							/* above assert has a >= instead of == due to possible
							 * ALIGN record in between */
							if (SS_NORMAL != (jnl_status = jnl_flush(reg)))
							{
								assert(NOJNL == jpc->channel); /* jnl file lost */
								REL_CRIT_BEFORE_RETURN(cnl, reg);
								send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2,
									JNL_LEN_STR(csd), ERR_TEXT, 2,
									RTS_ERROR_TEXT("Error with journal flush during wcs_flu2"),
									jnl_status);
								return FALSE;
							}
							assert(jb->freeaddr == jb->dskaddr);
							assert(jb->freeaddr == jb->rsrv_freeaddr);
							jnl_fsync(reg, jb->dskaddr);
							/* Use jb->fsync_dskaddr (instead of "fsync_dskaddr") below as the
							 * shared memory copy is more uptodate (could have been updated by
							 * "jnl_fsync" call above).
							 */
							assert(jb->fsync_dskaddr == jb->dskaddr);
						}
					}
					/* After the "wcs_recover" call above, it is possible a dirty cache-record which was
					 * in the wip queue and corresponded to a dead pid got re-inserted into the wip
					 * queue. In that case, the call to WAIT_FOR_WIP_QUEUE_CLEAR (which in turn calls
					 * "wcs_wtfini") would reinsert this into the active queue. So we need to call
					 * "wcs_wtstart"/WAIT_FOR_WIP_QUEUE_CLEAR once more to clean this out. Hence the
					 * loop count of 2 below.
					 */
					for (lcnt = 0; lcnt < 2; lcnt++)
					{
						wtstart_or_wtfini_errno = wcs_wtstart(reg, n_bts, NULL, NULL);	/* Flush it all */
						WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart, was_crit, reg, csa, cnl);
						CLEAR_WIP_QUEUE_IF_NEEDED(asyncio, wtstart_or_wtfini_errno, cnl, crwipq, reg, ret);
						if (!ret)
						{	/* We expect caller to trigger cache-recovery which will fix wip queue */
							return FALSE;
						}
						if (FLUSH_NOT_COMPLETE(cnl, crq, crwipq, n_bts))
						{
							if (!lcnt)
								continue;
							/* Something wrong inspite of all these attempts */
							REL_CRIT_BEFORE_RETURN(cnl, reg);
							assertpro(FALSE);
						}
					}
				}
			}
		}
	}
	if (flush_hdr)
		fileheader_sync(reg);
	if (jnl_enabled && write_epoch)
	{	/* If need to write an epoch,
		 *	(1) get hold of the jnl io_in_prog lock.
		 *	(2) set need_db_fsync to TRUE in the journal buffer.
		 *	(3) release the jnl io_in_prog lock.
		 *	(4) write an epoch record in the journal buffer.
		 * The next call to jnl_qio_start will do the fsync of the db before doing any jnl qio.
		 * The basic requirement is that we shouldn't write the epoch out until we have synced the database.
		 */
#		ifdef DEBUG
		if (!gtm_white_box_test_case_enabled || (WBTEST_JNL_FILE_LOST_DSKADDR != gtm_white_box_test_case_number))
		{
			assert(jb->rsrv_freeaddr == jb->fsync_dskaddr);
			assert(jb->rsrv_freeaddr == jb->freeaddr);
		}
#		endif
		/* If jb->need_db_fsync is TRUE at this point of time, it means we already have a db_fsync waiting
		 * to happen. This means the epoch due to the earlier need_db_fsync hasn't yet been written out to
		 * the journal file. But that means we haven't yet flushed the journal buffer which leads to a
		 * contradiction. (since we have called jnl_flush earlier in this routine and also assert to the
		 * effect jb->fsync_dskaddr == jb->rsrv_freeaddr a few lines above).
		 */
		assert(!jb->need_db_fsync);
		for (lcnt = 1; FALSE == (GET_SWAPLOCK(&jb->io_in_prog_latch)); lcnt++)
		{	/* this is a long lock and hence should be a mutex */
			if (MAXJNLQIOLOCKWAIT < lcnt)	/* tried too long */
			{
				GET_C_STACK_MULTIPLE_PIDS("MAXJNLQIOLOCKWAIT", cnl->wtstart_pid, MAX_WTSTART_PID_SLOTS, 1);
				assert(FALSE);
				REL_CRIT_BEFORE_RETURN(cnl, reg);
				assertpro(FALSE);
			}
			wcs_sleep(SLEEP_JNLQIOLOCKWAIT);	/* since it is a short lock, sleep the minimum */
			if ((MAXJNLQIOLOCKWAIT / 2 == lcnt) || (MAXJNLQIOLOCKWAIT == lcnt))
			{
				latch_salvaged = performCASLatchCheck(&jb->io_in_prog_latch, TRUE);
				if (latch_salvaged)
				{	/* jb->dskaddr & jb->dsk are updated while holding the io_in_prog_latch.
					 * Since the latch was salvaged, the holder pid could have been killed
					 * after jb->dskaddr has been updated but before jb->dsk has been updated
					 * (in "jnl_sub_qio_start"). Fix the discrepancy if any.
					 */
					jb->dsk = (jb->dskaddr % jb->size);
				}
			}
		}
		if (csd->jnl_before_image && !epoch_already_current)
			jb->need_db_fsync = TRUE;	/* for comments on need_db_fsync, see jnl_output_sp.c */
		/* else the journal files do not support before images and hence can only be used for forward recovery. So skip
		 * fsync of the database (jb->need_db_fsync = FALSE) because we don't care if the on-disk db is up-to-date or not.
		 * Also skip the fsync if we aren't actually going to write an epoch.
		 */
		RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		assert(!(JNL_FILE_SWITCHED(jpc)));
		assert(jgbl.gbl_jrec_time);
		if (!jgbl.mur_extract && !epoch_already_current)
		{
			if (0 == jpc->pini_addr)
				jnl_write_pini(csa);
			JNL_WRITE_EPOCH_REC(csa, cnl, clean_dbsync);
		} else if (epoch_already_current)
			jb->next_epoch_time = MAXUINT4;
	}
	cnl->last_wcsflu_tn = csa->ti->curr_tn;	/* record when last successful wcs_flu occurred */
	REL_CRIT_BEFORE_RETURN(cnl, reg);
	/* sync the epoch record in the journal if needed. */
	if (jnl_enabled && write_epoch && sync_epoch && (csa->ti->curr_tn == csa->ti->early_tn))
	{	/* Note that if we are in the midst of committing and came here through a bizarre
		 * stack trace (like wcs_get_space etc.) we want to defer syncing to when we go out of crit.
		 * Note that we are guaranteed to come back to wcs_wtstart since we are currently in commit-phase
		 * and will dirty atleast one block as part of the commit for a wtstart timer to be triggered.
		 */
		jnl_wait(reg);
	}
	if (need_db_fsync && JNL_ALLOWED(csd))
	{
		if (dba_mm != csd->acc_meth)
		{
			DB_FSYNC(reg, udi, csa, db_fsync_in_prog, save_errno);
			if (0 != save_errno)
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg), save_errno);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg), save_errno);
				assert(FALSE);	/* should not come here as the rts_error above should not return */
				return FALSE;
			}
		}
	}
	return TRUE;
}
