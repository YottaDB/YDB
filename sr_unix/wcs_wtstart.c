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
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_signal.h"	/* needed for VSIG_ATOMIC_T */
#include "gtm_stdio.h"

#include "aswp.h"
#include "copy.h"
#include "error.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "iosp.h"	/* required for SS_NORMAL for use with msyncs */
#include "interlock.h"
#include "io.h"
#include "gdsbgtr.h"
#include "aio_shim.h"
#include "gtmio.h"
#include "relqueopi.h"
#include "gt_timer.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "add_inter.h"
#include "wcs_recover.h"
#include "gtm_string.h"
#include "have_crit.h"
#include "gds_blk_downgrade.h"
#include "deferred_signal_handler.h"
#include "memcoherency.h"
#include "wbox_test_init.h"
#include "wcs_clean_dbsync.h"
#include "anticipatory_freeze.h"
#include "gtmcrypt.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "t_retry.h"
#include "min_max.h"
#include "gtmimagename.h"
#include "util.h"
#include "gtm_multi_proc.h"	/* for "multi_proc_in_use" GBLREF */
#include "wcs_backoff.h"
#include "wcs_wt.h"
#include "performcaslatchcheck.h"
#include "wcs_sleep.h"
#include "caller_id.h"
#include "rel_quant.h"
#include "sleep_cnt.h"
#include "gtm_c_stack_trace.h"
#include "relqop.h"


#ifdef DEBUG
GBLREF		int4		exit_state;

STATICDEF	int		wcs_wtstart_count;

/* White-box-test-activated macro to sleep in one of the predetermined places (based on the count variable)
 * inside wcs_wtstart. The sleep allows for the delivery of an interrupt in a specific window of code.
 */
#  define SLEEP_ON_WBOX_COUNT(COUNT)								\
{												\
	if (WBTEST_ENABLED(WBTEST_SLEEP_IN_WCS_WTSTART)						\
		&& (COUNT == (gtm_white_box_test_case_count % 100)))				\
	{											\
		if ((gtm_white_box_test_case_count / 100) == ++wcs_wtstart_count)		\
		{	/* Resetting this allows us to avoid redundant sleeps while having the	\
			 * white-box logic variables still enabled (to avoid asserts).		\
			 */									\
			gtm_white_box_test_case_count = 0;					\
			DBGFPF((stderr, "WCS_WTSTART: STARTING SLEEP\n"));			\
			while (TRUE)								\
			{									\
				SHORT_SLEEP(999);						\
				if (0 < exit_state)						\
					DBGFPF((stderr, "exit_state is %d\n", exit_state));	\
			}									\
		}										\
	}											\
}
#else
#  define SLEEP_ON_WBOX_COUNT(COUNT)
#endif

GBLREF	uint4			process_id;
GBLREF	sm_uc_ptr_t		reformat_buffer;
GBLREF	int			reformat_buffer_len;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data		*cs_data;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			update_trans;
GBLREF	uint4			mu_reorg_encrypt_in_prog;
GBLREF	sgmnt_addrs		*reorg_encrypt_restart_csa;
GBLREF	bool			in_mupip_freeze;
#ifdef DEBUG
GBLREF	volatile int		reformat_buffer_in_use;
GBLREF	volatile int4		gtmMallocDepth;
#endif
GBLREF	volatile int4		fast_lock_count;

error_def(ERR_DBCCERR);
error_def(ERR_ENOSPCQIODEFER);
error_def(ERR_GBLOFLOW);
error_def(ERR_JNLFSYNCERR);
error_def(ERR_JNLWRTDEFER);
error_def(ERR_JNLWRTNOWWRTR);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

int4	wcs_wtstart(gd_region *region, int4 writes, wtstart_cr_list_t *cr_list_ptr, cache_rec_ptr_t cr2flush)
{
	blk_hdr_ptr_t		bp, save_bp;
	boolean_t               need_jnl_sync, queue_empty, got_lock, bmp_status, do_asyncio, wtfini_called_once;
	cache_que_head_ptr_t	ahead, whead;
	cache_state_rec_ptr_t	csr, csrfirst;
	int4                    err_status = 0, n, n1, n2, max_ent, max_writes, save_errno;
        size_t                  size ;
	jnl_buffer_ptr_t        jb;
        jnl_private_control     *jpc;
	node_local_ptr_t	cnl;
	off_t			blk_1_off, offset;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_ptr;
	uint4			saved_dsk_addr;
	unix_db_info		*udi;
	cache_rec_ptr_t		cr, cr_lo, cr_hi;
	static	int4		error_message_loop_count = 0;
	uint4			index;
	boolean_t		is_mm, was_crit;
	uint4			curr_wbox_seq_num;
	int			try_sleep, rc;
	gd_region		*sav_cur_region;
	sgmnt_addrs		*sav_cs_addrs;
	sgmnt_data		*sav_cs_data;
	jnlpool_addrs_ptr_t	sav_jnlpool;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* needed by INST_FREEZE_ON_ERROR_POLICY_CSA */
	intrpt_state_t		prev_intrpt_state;
	char			*in, *out;
	int			in_len;
	int4			gtmcrypt_errno = 0;
	gd_segment		*seg;
	boolean_t		use_new_key, skip_in_trans, skip_sync, sync_keys;
	que_ent_ptr_t		next, prev;
	void_ptr_t              retcsrptr;
	boolean_t		keep_buff_lock, pushed_region;
	cache_rec_ptr_t		older_twin;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (cr_list_ptr)
		cr_list_ptr->numcrs = 0;
	udi = FILE_INFO(region);
	csa = &udi->s_addrs;
	pushed_region = INST_FREEZE_ON_ERROR_POLICY_CSA(csa, local_jnlpool);
	if (pushed_region)
		PUSH_GV_CUR_REGION(region, sav_cur_region, sav_cs_addrs, sav_cs_data, sav_jnlpool);
	csd = csa->hdr;
	is_mm = (dba_mm == csd->acc_meth);
	assert(is_mm || (dba_bg == csd->acc_meth));
	BG_TRACE_PRO_ANY(csa, wrt_calls);	/* Calls to wcs_wtstart */
	/* If this process is already in wcs_wtstart for this region, we won't interrupt it again */
	cnl = csa->nl;
	if (csa->in_wtstart)
	{
		WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart1, 0, 0, 0, 0, 0);
		BG_TRACE_PRO_ANY(csa, wrt_busy);
		if (pushed_region)
			POP_GV_CUR_REGION(sav_cur_region, sav_cs_addrs, sav_cs_data, sav_jnlpool);
		return err_status;			/* Already here, get out */
	}
	/* Defer interrupts to protect against an inconsistent state caused by mismatch of such values as
	 * cnl->intent_wtstart and cnl->in_wtstart.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_WCS_WTSTART, prev_intrpt_state);
	INCR_INTENT_WTSTART(cnl);	/* signal intent to enter wcs_wtstart */
	/* the above interlocked instruction does the appropriate write memory barrier to publish this change to the world */
	SHM_READ_MEMORY_BARRIER;	/* need to do this to ensure uptodate value of cnl->wc_blocked is read */
	if (cnl->wc_blocked)
	{
		WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart2, 0, 0, 0, 0, 0);
		DECR_INTENT_WTSTART(cnl);
		BG_TRACE_PRO_ANY(csa, wrt_blocked);
		if (pushed_region)
			POP_GV_CUR_REGION(sav_cur_region, sav_cs_addrs, sav_cs_data, sav_jnlpool);
		ENABLE_INTERRUPTS(INTRPT_IN_WCS_WTSTART, prev_intrpt_state);
		return err_status;
	}
	SLEEP_ON_WBOX_COUNT(1);
	csa->in_wtstart = TRUE;				/* Tell ourselves we're here and make the csa->in_wtstart (private copy) */
	/* Ideally, we would like another SLEEP_ON_WBOX_COUNT here, but that could cause assert failures in concurrent wcs_wtstarts.
	 * Because it is highly unlikely for an interrupt-deferred process to get killed at exactly this spot, do not test that.
	 */
	INCR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);	/* and cnl->in_wtstart (shared copy) assignments as close as possible.   */
	if (FROZEN_CHILLED(csa) && !FREEZE_LATCH_HELD(csa))
	{
		CAREFUL_DECR_CNT(cnl->in_wtstart, cnl->wc_var_lock);
		DECR_INTENT_WTSTART(cnl);
		csa->in_wtstart = FALSE;
		if (pushed_region)
			POP_GV_CUR_REGION(sav_cur_region, sav_cs_addrs, sav_cs_data, sav_jnlpool);
		ENABLE_INTERRUPTS(INTRPT_IN_WCS_WTSTART, prev_intrpt_state);
		/* Return non-zero in order to break wcs_wtstart_fini() out of its loop. Ignored elsewhere. */
		return EAGAIN;
	}
	SLEEP_ON_WBOX_COUNT(2);
	SAVE_WTSTART_PID(cnl, process_id, index);
	assert((cnl->in_wtstart > 0) && csa->in_wtstart);
	max_ent = csd->n_bts;
	if (0 == (max_writes = writes))			/* If specified writes to do, use that.. */
		max_writes = csd->n_wrt_per_flu;	/* else, max writes is how many blocks there are */
	jpc = csa->jnl;
	assert(!JNL_ALLOWED(csd) ||( NULL != jpc));	/* if journaling is allowed, we better have non-null csa->jnl */
	if (JNL_ENABLED(csd) && (NULL != jpc) && (NOJNL != jpc->channel))
	{	/* Before flushing the database buffers, give journal flushing a nudge. Any failures in writing to the
		 * journal are not handled here since the main purpose of wcs_wtstart is to flush the database buffers
		 * (not journal buffers). The journal issue will be caught later (in jnl_flush or some other jnl routine)
		 * and appropriate errors, including triggering jnl_file_lost (if JNLCNTRL error) will be issued there.
		 */
		jnl_qio_start(jpc);
	}
	if (is_mm)
	{
		queue_empty = TRUE;
		n1 = 1; /* set to a non-zero value so dbsync timer canceling (if needed) can happen */
		goto writes_completed; /* to avoid unnecessary IF checks in the more common case (BG) */
	}
	ahead = &csa->acc_meth.bg.cache_state->cacheq_active;
	whead = &csa->acc_meth.bg.cache_state->cacheq_wip;
	cr_lo = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	cr_hi = cr_lo + csd->n_bts;
	assert(((sm_long_t)ahead & 7) == 0);
	queue_empty = FALSE;
	csa->wbuf_dqd++;			/* Tell rundown we have an orphaned block in case of interrupt */
	SLEEP_ON_WBOX_COUNT(3);
	was_crit = csa->now_crit;
	SLEEP_ON_WBOX_COUNT(4);
	skip_in_trans = FALSE;
	assert(!is_mm);	/* MM should have bypassed this "for" loop completely */
	wtfini_called_once = FALSE;
	WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart3, 0, 0, 0, 0, 0);
	for (n1 = n2 = 0, csrfirst = NULL; (n1 < max_ent) && (n2 < max_writes) && !cnl->wc_blocked; ++n1)
	{	/* If not-crit, avoid REMQHI by peeking at the active queue and if it is found to have a 0 fl link, assume
		 * there is nothing to flush and break out of the loop. This avoids unnecessary interlock usage (GTM-7635).
		 * If holding crit, we cannot safely avoid the REMQHI so interlock usage is avoided only in the no-crit case.
		 */
		if (!was_crit && (0 == ahead->fl))
			csr = NULL;
		keep_buff_lock = FALSE;
		if (cr2flush)
		{ 	/* asked to flush a specific cr: */
			/* should be dirty and not have had a write issued, i.e., in the active queue */
			max_ent = 1;
			max_writes = 1;
			csr = NULL; /* assume it's none until we find it */
			if (cr2flush->dirty && !cr2flush->epid)
			{ 	/* if it is in the active queue */
				++fast_lock_count; /* Disable wcs_stale for duration */
				if (grab_latch(&ahead->latch, WT_LATCH_TIMEOUT_SEC))
				{
					cr = cr2flush;
					csr = (cache_state_rec_ptr_t)((sm_uc_ptr_t)cr + SIZEOF(cr->blkque));
					if (csr->dirty && !csr->epid && csr->state_que.fl)
					{	/* Now that we know csr is in the active queue, remove it. */
						retcsrptr = remqh((que_ent_ptr_t)((sm_uc_ptr_t)&csr->state_que
							+ csr->state_que.bl));
						if ((cache_state_rec_ptr_t)retcsrptr != csr)
                                                {	/* Did not get the csr we intended so something must be wrong with cache.
							 * Kill -9 can cause this. Assert that we were doing a crash shutdown.
							 */
                                                        assert(gtm_white_box_test_case_enabled
                                                                && (WBTEST_CRASH_SHUTDOWN_EXPECTED
                                                                == gtm_white_box_test_case_number));
                                                        SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
                                                        err_status = ERR_DBCCERR;
                                                        break;
                                                }
						csr->state_que.fl = (sm_off_t)0;
						csr->state_que.bl = (sm_off_t)0;
						/* LOCK_BUFF_FOR_WRITE needs to happens AFTER the remqh just like
						 * the non-cr2flush case because bg_update_phase2() relies on this
						 * ordering for reinserting a cr into the active queue.
						 */
						LOCK_BUFF_FOR_WRITE(csr, n, &cnl->db_latch);
						assert(WRITE_LATCH_VAL(csr) >= LATCH_CLEAR);
						assert(WRITE_LATCH_VAL(csr) <= LATCH_CONFLICT);
						if (OWN_BUFF(n))
						{
							assert(WRITE_LATCH_VAL(csr) > LATCH_CLEAR);
							assert(0 == n);
							keep_buff_lock = TRUE;
						} else
							csr = NULL; /* another process is taking care of this cr */
					} else
						csr = NULL; /* no longer on the active queue */
					rel_latch(&ahead->latch);
				} else
					csr = NULL; /* did not get the lock */
        			--fast_lock_count;
        			assert(0 <= fast_lock_count);
			}
		} else
		{
			csr = (cache_state_rec_ptr_t)REMQHI((que_head_ptr_t)ahead);
			if (INTERLOCK_FAIL == (INTPTR_T)csr)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtstart_lckfail1);
				break;
			}
			cr = (cache_rec_ptr_t)((sm_uc_ptr_t)csr - SIZEOF(cr->blkque));
		}
		if (NULL == csr)
			break;				/* the queue is empty */
		assert(!FROZEN_CHILLED(csa) || FREEZE_LATCH_HELD(csa));
		if (csr == csrfirst)
		{					/* completed a tour of the queue */
			queue_empty = FALSE;
			assert(!keep_buff_lock);
			/* the if check and lock clear is for PRO just in case */
			if (keep_buff_lock)
				CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
			REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail2);
			if (INTERLOCK_FAIL == n)
				err_status = ERR_DBCCERR;
			break;
		}
		assert(!CR_NOT_ALIGNED(cr, cr_lo) && !CR_NOT_IN_RANGE(cr, cr_lo, cr_hi));
		if (CR_BLKEMPTY == csr->blk)
		{	/* must be left by t_commit_cleanup - removing it from the queue and the following
			 * completes the cleanup
			 */
			WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart4, cr->blk, GDS_ANY_ABS2REL(csa,cr), cr->dirty, 0, 0);
			assert(0 != csr->dirty);
			assert(csr->data_invalid);
			csr->data_invalid = FALSE;
			csr->dirty = 0;
			assert(!keep_buff_lock);
			/* the if check and lock clear is for PRO just in case */
			if (keep_buff_lock)
				CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
			ADD_ENT_TO_FREE_QUE_CNT(cnl);
			assert(LATCH_CLEAR == WRITE_LATCH_VAL(csr));
			queue_empty = !SUB_ENT_FROM_ACTIVE_QUE_CNT(cnl);
			continue;
		}
		/* If journaling, write only if the journal file is up to date and no jnl-switches occurred */
		if (JNL_ENABLED(csd))
		{	/* this looks to be a long lock and hence should use a mutex */
			jb = jpc->jnl_buff;
			need_jnl_sync = (csr->jnl_addr > jb->fsync_dskaddr);
			assert(!need_jnl_sync || ((NOJNL) != jpc->channel) || (cnl->wcsflu_pid != process_id));
			got_lock = FALSE;
			if ((csr->jnl_addr > jb->dskaddr)
			    || (need_jnl_sync && (NOJNL == jpc->channel
					|| (FALSE == (got_lock = GET_SWAPLOCK(&jb->fsync_in_prog_latch))))))
			{
				WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart5, cr->blk, GDS_ANY_ABS2REL(csa,cr), cr->dirty,	\
					need_jnl_sync, got_lock);
				if (need_jnl_sync)
					BG_TRACE_PRO_ANY(csa, n_jnl_fsync_tries);
				if (keep_buff_lock)
					CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
				REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail3);
				if (INTERLOCK_FAIL == n)
				{
					err_status = ERR_DBCCERR;
					break;
				}
				if (NULL == csrfirst)
					csrfirst = csr;
				continue;
			} else if (got_lock)
			{
				saved_dsk_addr = jb->dskaddr;
				if (jpc->sync_io)
				{
					/* We need to maintain the fsync control fields irrespective of the type of IO,
					 * because we might switch between these at any time.
					 */
					jb->fsync_dskaddr = saved_dsk_addr;
				} else
				{
					GTM_JNL_FSYNC(csa, jpc->channel, rc);
					if (-1 == rc)
					{
						assert(FALSE);
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFSYNCERR, 2, JNL_LEN_STR(csd),
							 ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), errno);
						RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
						if (keep_buff_lock)
							CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
						REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail3);
						if (INTERLOCK_FAIL == n)
						{
							err_status = ERR_DBCCERR;
							break;
						}
						if (NULL == csrfirst)
							csrfirst = csr;
						continue;
					} else
					{
						jb->fsync_dskaddr = saved_dsk_addr;
						BG_TRACE_PRO_ANY(csa, n_jnl_fsyncs);
					}
				}
				RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
			}
		}
		/* If twin exists then do not issue write of NEWER twin until OLDER twin has been removed from WIP queue.
		 * The act of removal from the WIP queue clears "csr->twin" so checking just that is enough.
		 */
		if (csr->twin)
		{
			assert(csd->asyncio);	/* Assert that ASYNCIO is turned ON as that is a necessity for twinning */
			/* Check if crit can be obtained right away. If so, call "wcs_wtfini" after getting crit.
			 * And recheck if the "twin" has been broken. If so proceed with the write. Else skip this write.
			 * Do not call heavyweight "wcs_wtfini" more than once per "wcs_wtstart" call.
			 * Also we are meddling with active queue now so we cannot risk a "wcs_recover" call inside
			 * "grab_crit_immediate" hence the OK_FOR_WCS_RECOVER_FALSE usage below.
			 */
			if (!wtfini_called_once && (was_crit || grab_crit_immediate(region, OK_FOR_WCS_RECOVER_FALSE)))
			{
				if (csr->twin)
				{
					DEBUG_ONLY(dbg_wtfini_lcnt = dbg_wtfini_wcs_wtstart);	/* used by "wcs_wtfini" */
					older_twin = (csr->bt_index ? (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, csr->twin) : cr);
					assert(!older_twin->bt_index);
					wcs_wtfini(region, CHECK_IS_PROC_ALIVE_FALSE, older_twin);
					wtfini_called_once = TRUE;
				}
				if (!was_crit)
					rel_crit(region);
			}
			/* Note that in the most common case, csr will be the NEWER twin. But it is possible csr is the OLDER
			 * twin too. For example, if the OLDER twin's write got aborted because the process that initiated
			 * the write got killed and "wcs_wtfini" moved the csr back into the active queue. csr->bt_index
			 * being non-zero indicates it is a NEWER twin in which case we need to wait for the twin link to be broken.
			 */
			if (csr->twin && csr->bt_index)
			{
				WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart6, cr->blk, GDS_ANY_ABS2REL(csa,cr), cr->dirty,	\
					cr->bt_index, 0);
				if (keep_buff_lock)
					CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
				REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail3);
				if (INTERLOCK_FAIL == n)
				{
					err_status = ERR_DBCCERR;
					break;
				}
				if (NULL == csrfirst)
					csrfirst = csr;
				continue;
			}
		}
		csr->aio_issued = FALSE;	/* set this to TRUE before csr->epid is set to a non-zero value.
						 * To avoid out-of-order execution place this BEFORE the LOCK_BUFF_FOR_WRITE.
						 * It does not hurt in the case we skip the "if (OWN_BUFF(n))" check.
						 */
		if (!keep_buff_lock)
			LOCK_BUFF_FOR_WRITE(csr, n, &cnl->db_latch);
		else
			assert(OWN_BUFF(n)); /* since we keep it we better own it */
		assert(WRITE_LATCH_VAL(csr) >= LATCH_CLEAR);
		assert(WRITE_LATCH_VAL(csr) <= LATCH_CONFLICT);
		if (OWN_BUFF(n))
		{	/* sole owner */
			assert(csr->dirty);
			assert(WRITE_LATCH_VAL(csr) > LATCH_CLEAR);
			assert(0 == n);
			/* We're going to write this block out now */
			save_errno = 0;
			assert(FALSE == csr->data_invalid);	/* check that buffer has valid data */
			csr->epid = process_id;
			CR_BUFFER_CHECK1(region, csa, csd, cr, cr_lo, cr_hi);
			bp = (blk_hdr_ptr_t)(GDS_ANY_REL2ABS(csa, csr->buffaddr));
			VALIDATE_BM_BLK(csr->blk, bp, csa, region, bmp_status);	/* bmp_status holds bmp buffer's validity */
			/* GDSV4 (0) version uses this field as a block length so should always be > 0. Assert that.
			 * There is one exception in that we might have a crash test where a process was killed just before
			 * populating a block in bg_update_phase2 when cr->data_invalid is still 0 but cr->in_tend is non-zero
			 * (so the block-header is still null) in which case "wcs_recover" would have kept cr->dirty non-zero
			 * even though the block-header is empty. So assert that. Note that gds_blk_downgrade has a safety
			 * check for bver == 0 and returns immediately in that case so it is okay to call it with a 0 bver in pro.
			 */
			assert(((blk_hdr_ptr_t)bp)->bver
				|| (gtm_white_box_test_case_enabled
					&& (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number)));
			if (IS_GDS_BLK_DOWNGRADE_NEEDED(csr->ondsk_blkver))
			{	/* Need to downgrade/reformat this block back to a previous format. */
				assert(!csd->asyncio);	/* asyncio & V4 format are not supported together */
				assert((0 == reformat_buffer_in_use) || process_exiting);
				DEBUG_ONLY(reformat_buffer_in_use++;)
				DEBUG_DYNGRD_ONLY(PRINTF("WCS_WTSTART: Block %d being dynamically downgraded on write\n", \
							 csr->blk));
				if (csd->blk_size > reformat_buffer_len)
				{	/* Buffer not big enough (or does not exist) .. get a new one releasing
					 * old if it exists
					 */
					assert(0 == gtmMallocDepth);	/* should not be in a nested free/malloc */
					if (reformat_buffer)
						free(reformat_buffer);	/* Different blksized databases in use
									   .. keep only largest one */
					reformat_buffer = malloc(csd->blk_size);
					reformat_buffer_len = csd->blk_size;
				}
				gds_blk_downgrade((v15_blk_hdr_ptr_t)reformat_buffer, (blk_hdr_ptr_t)bp);
				bp = (blk_hdr_ptr_t)reformat_buffer;
				size = (((v15_blk_hdr_ptr_t)bp)->bsiz + 1) & ~1;
			} else DEBUG_ONLY(if (GDSV6 == csr->ondsk_blkver))
				size = (bp->bsiz + 1) & ~1;
#			ifdef DEBUG
			else
				assert(IS_GDS_BLK_DOWNGRADE_NEEDED(csr->ondsk_blkver) || (GDSV6 == csr->ondsk_blkver));
#			endif
			if (csa->do_fullblockwrites)
			{	/* See similiar logic in wcs_wtstart.c */
				size = (int)ROUND_UP(size,
						(FULL_DATABASE_WRITE == csa->do_fullblockwrites && csr->needs_first_write)
						? csd->blk_size : csa->fullblockwrite_len);
			}
			assert(size <= csd->blk_size);
			INCR_GVSTATS_COUNTER(csa, cnl, n_dsk_write, 1);
			save_bp = bp;
			/* Encryption settings in the database file header cannot change at this time because a concurrent
			 * MUPIP REORG -ENCRYPT process should wait for all ongoing wcs_wtstarts to finish before
			 * proceeding. Therefore, we can safely reference csd to (re)initialize the encryption handles based
			 * on the hashes in the file header.
			 */
			use_new_key = USES_NEW_KEY(csd);
			if (IS_ENCRYPTED(csd->is_encrypted) || use_new_key)
			{
				seg = region->dyn.addr;
				assert(NULL != csa->encr_ptr);
				skip_sync = FALSE;
				sync_keys = FALSE;
				if (csa->encr_ptr->reorg_encrypt_cycle != cnl->reorg_encrypt_cycle)
				{
					assert(!mu_reorg_encrypt_in_prog);
					if (IS_NOT_SAFE_TO_SYNC_NEW_KEYS(dollar_tlevel, update_trans))
						skip_sync = TRUE;
					else
					{
						sync_keys = TRUE;
						assert(NULL == reorg_encrypt_restart_csa);
					}
				} else if (NULL != reorg_encrypt_restart_csa)
				{	/* The reorg_encrypt_cycle fields are identical (between csa->encr_ptr and cnl), but
					 * the global variable reorg_encrypt_restart_csa indicates one of two possibilities.
					 * a) We are in the middle of a transaction-retry due to cdb_sc_reorg_encrypt status
					 *	code and t_retry/tp_restart will take care of doing the reinitialization of
					 *	the new key handles. We cannot do the wcs_wtstart until then in case we encounter
					 *	a block with the new key. Skip this wcs_wtstart call as if the cycles were
					 *	different.
					 * b) We are exiting i.e. "process_exiting" = TRUE. In that case, we are clearly not in
					 *	the middle of a transaction that will be committed. And so, we can safely go
					 *	ahead and (re)initialize the encryption handles. And proceed with the flush of
					 *	the buffers using uptodate encryption keys.
					 */
					if (process_exiting)
						sync_keys = TRUE;
					else
						skip_sync = TRUE;
				}
				if (skip_sync)
				{
					DBG_RECORD_BLOCK_ABORT(csd, csa, cnl, process_id);
					skip_in_trans = TRUE;
				}
				if (sync_keys)
				{	/* Note: Below logic is very similar to "process_reorg_encrypt_restart" but we do
					 * not invoke that function here because it assumes various things (e.g. non-NULL
					 * "reorg_encrypt_restart_csa", no crit on any region etc.) all of which are not
					 * guaranteed in some cases.
					 */
					INIT_DB_OR_JNL_ENCRYPTION(csa, csd, seg->fname_len, seg->fname, gtmcrypt_errno);
					save_errno = gtmcrypt_errno;
					if (0 == save_errno)
						COPY_ENC_INFO(csd, csa->encr_ptr, cnl->reorg_encrypt_cycle);
					reorg_encrypt_restart_csa = NULL; /* Reset this in case it is non-NULL */
				}
				if (!skip_in_trans && (0 == save_errno))
				{
					assert((unsigned char *)bp != reformat_buffer);
					DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, (sm_uc_ptr_t)bp);
					save_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, csa);
					DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, (sm_uc_ptr_t)save_bp);
					assert((bp->bsiz <= csd->blk_size) && (bp->bsiz >= SIZEOF(*bp)));
					in_len = MIN(csd->blk_size, bp->bsiz) - SIZEOF(*bp);
					if (BLK_NEEDS_ENCRYPTION(bp->levl, in_len))
					{
						ASSERT_ENCRYPTION_INITIALIZED;
						memcpy(save_bp, bp, SIZEOF(blk_hdr));
						in = (char *)(bp + 1);
						out = (char *)(save_bp + 1);
						if (use_new_key)
						{
							GTMCRYPT_ENCRYPT(csa, TRUE, csa->encr_key_handle2, in, in_len, out,
									bp, SIZEOF(blk_hdr), gtmcrypt_errno);
						} else
						{
							GTMCRYPT_ENCRYPT(csa, csd->non_null_iv, csa->encr_key_handle, in,
									in_len, out, bp, SIZEOF(blk_hdr), gtmcrypt_errno);
						}
						DBG_RECORD_BLOCK_WRITE(csd, csa, cnl, process_id, csr->blk,
							((blk_hdr *)bp)->tn,
							4, use_new_key, bp, save_bp, bp->bsiz, in_len);
						save_errno = gtmcrypt_errno;
					} else
					{
						memcpy(save_bp, bp, bp->bsiz);
						DBG_RECORD_BLOCK_WRITE(csd, csa, cnl, process_id, csr->blk,
							((blk_hdr *)bp)->tn,
							5, use_new_key, bp, save_bp, bp->bsiz, in_len);
					}
				}
			} else
			{
				DBG_RECORD_BLOCK_WRITE(csd, csa, cnl, process_id, csr->blk,
					((blk_hdr *)bp)->tn,
					6, use_new_key, bp, save_bp, bp->bsiz, 0);
			}
			/* If online rollback has forked off child processes to operate on each region,
			 * we have seen ASYNC IOs issued from the child process do not finish for reasons unknown.
			 * So we disable asyncio in the forward phase of offline/online rollback/recover.
			 * This is easily identified currently by the global variable "multi_proc_in_use" being TRUE.
			 */
#			ifdef USE_NOAIO
			do_asyncio = FALSE;
#			else
			do_asyncio = csd->asyncio && !multi_proc_in_use;
#			endif
			if (udi->fd_opened_with_o_direct)
			{
				size = ROUND_UP2(size, DIO_ALIGNSIZE(udi));
				assert(size <= csd->blk_size);
			}
			if (!skip_in_trans && (0 == save_errno))
			{	/* Due to csa->in_wtstart protection (at the beginning of this module), we are guaranteed
				 * that the write below won't be interrupted by another nested wcs_wtstart
				 */
				offset = BLK_ZERO_OFF(csd->start_vbn) + (off_t)csr->blk * csd->blk_size;
				if (!do_asyncio)
				{
					DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, offset, save_bp, size, save_errno);
					csr->needs_first_write = FALSE;
				} else
				{
					cr->wip_is_encr_buf = (save_bp != bp);
					DB_LSEEKWRITEASYNCSTART(csa, udi, udi->fn, udi->fd, offset, save_bp, size, cr, save_errno);
					if (EAGAIN == save_errno)
					{	/* ASYNCIO IO could not be started due to OS not having enough memory temporarily */
						BG_TRACE_PRO_ANY(csa, wcs_wtstart_eagain);
						if (was_crit)
						{	/* Holding crit. Do synchronous IO as we need this flushed. */
							do_asyncio = FALSE;
							BG_TRACE_PRO_ANY(csa, wcs_wtstart_eagain_incrit);
							DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, offset,		\
											save_bp, size, save_errno);
							csr->needs_first_write = FALSE;
						}
						/* else: We do not hold crit so flushing this is not critical. */
					} else if (0 == save_errno)
						csr->aio_issued = TRUE;
				}
			}
			if ((blk_hdr_ptr_t)reformat_buffer == bp)
			{
				DEBUG_ONLY(reformat_buffer_in_use--;)
				assert((0 == reformat_buffer_in_use) || process_exiting);
			}
			/* Trigger I/O error if white box test case is turned on */
			GTM_WHITE_BOX_TEST(WBTEST_WCS_WTSTART_IOERR, save_errno, ENOENT);
			if (skip_in_trans || (0 != save_errno))
			{
				WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart7, cr->blk, GDS_ANY_ABS2REL(csa,cr), cr->dirty,	\
					skip_in_trans, save_errno);
				assert((ERR_ENOSPCQIODEFER != save_errno) || !was_crit || skip_in_trans);
				csr->epid = 0; /* before releasing update lock, clear epid */
				CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
				REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail4);
				if (INTERLOCK_FAIL == n)
				{
					err_status = ERR_DBCCERR;
					break;
				}
				err_status = save_errno;
				if (!skip_in_trans)
				{	/* We have an error from the write. Could be disk space or a real error. Handle it.
					 * Note: This write will be automatically retried after csd->flush_time[0] msec, if this
					 * was called through a timer-pop, otherwise, error (return value from this function)
					 * should be handled (including ignored) by the caller.
					 */
					wcs_wterror(region, save_errno);
				} else
					assert(0 == save_errno);
				break;
			} else if (do_asyncio)
			{
				n = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)whead);
				if (INTERLOCK_FAIL == n)
				{
					assert(FALSE);
					csr->epid = 0;
					SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wcb_wtstart_lckfail4);
					err_status = ERR_DBCCERR;
					break;
				}
				if (cr_list_ptr) /* we've been asked to return a list of crs where we issued i/o's */
				{
					assert(cr_list_ptr->numcrs < cr_list_ptr->listsize);
					cr_list_ptr->listcrs[cr_list_ptr->numcrs++] = cr;
				}
				ADD_ENT_TO_WIP_QUE_CNT(cnl);
			}
			cnl->wtstart_errcnt = 0; /* Discard any previously noted I/O errors */
			++n2;
			BG_TRACE_ANY(csa, wrt_count);
			/* Detect whether queue has become empty. Defer action (calling wcs_clean_dbsync)
			 * to end of routine, since we still hold the lock on the cache-record
			 */
			queue_empty = !SUB_ENT_FROM_ACTIVE_QUE_CNT(cnl);
			if (!do_asyncio)
			{
				csr->flushed_dirty_tn = csr->dirty;
				csr->epid = 0;
				ADD_ENT_TO_FREE_QUE_CNT(cnl);
				csr->dirty = 0;
				/* Even though asyncio is ON we may have done a synchronous I/O to get it done, e.g.,
				 * we were holding crit and got an asyncio error. If that is the case, check for
				 * a twin.
				 */
				if (csd->asyncio && csr->twin)
					BREAK_TWIN(csr, csa);
				CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
				/* Note we are still under protection of wbuf_dqd lock at this point. Reason we keep
				 * it so long is so that all the counters are updated along with the queue being correct.
				 * The result of not doing this previously is that wcs_recover was NOT called when we
				 * got interrupted just prior to the counter adjustment leaving wcs_active_lvl out of
				 * sync with the actual count on the queue which caused an assert failure in wcs_flu. SE 11/2000
				 */
			}
		} else
			WCS_OPS_TRACE(csa, process_id, wcs_ops_wtstart8, cr->blk, GDS_ANY_ABS2REL(csa,cr), cr->dirty, n, 0);
	}
	csa->wbuf_dqd--;
writes_completed:
	DEBUG_ONLY(
		if (0 == n2)
			BG_TRACE_ANY(csa, wrt_noblks_wrtn);
		assert((cnl->in_wtstart > 0) && csa->in_wtstart);
	)
	SLEEP_ON_WBOX_COUNT(5);
	if (csa->dbsync_timer && n1)
	{	/* If we already have a dbsync timer active AND we found at least one dirty cache record in the active queue
		 * now, this means there has not been enough time period of idleness since the last update and so there is
		 * no purpose to the existing timer. A new one would anyways be started whenever the last dirty cache
		 * record in the current active queue is flushed. Cancel the previous one.
		 */
		CANCEL_DBSYNC_TIMER(csa);
	}
	CAREFUL_DECR_CNT(cnl->in_wtstart, cnl->wc_var_lock);
	/* Ideally, we would like another SLEEP_ON_WBOX_COUNT here, but that could cause assert failures in concurrent wcs_wtstarts.
	 * Because it is highly unlikely for an interrupt-deferred process to get killed at exactly this spot, do not test that.
	 */
	CLEAR_WTSTART_PID(cnl, index);
	csa->in_wtstart = FALSE;			/* This process can write again */
	SLEEP_ON_WBOX_COUNT(6);
	DECR_INTENT_WTSTART(cnl);
	SLEEP_ON_WBOX_COUNT(7);
	if (queue_empty)			/* Active queue has become empty. */
		wcs_clean_dbsync_timer(csa);	/* Start a timer to flush-filehdr (and write epoch if before-imaging) */
	ENABLE_INTERRUPTS(INTRPT_IN_WCS_WTSTART, prev_intrpt_state);
	if (0 != gtmcrypt_errno)
	{	/* Now that we have done all cleanup (reinserted the cache-record that failed the write and cleared cnl->in_wtstart
		 * and cnl->intent_wtstart, go ahead and issue the error.
		 */
		GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
	}
	if (pushed_region)

		POP_GV_CUR_REGION(sav_cur_region, sav_cs_addrs, sav_cs_data, sav_jnlpool);
	return err_status;
}
