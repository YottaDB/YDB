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

#include "gtm_string.h"
#include "gtm_time.h"
#include "gtmimagename.h"

#include <sys/mman.h>
#ifdef _AIX
# include <sys/shm.h>
#endif
#include "gtm_stat.h"
#include <errno.h>
#include "gtm_signal.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "filestruct.h"
#include "gdscc.h"
#include "interlock.h"
#include "jnl.h"
#include "testpt.h"
#include "sleep_cnt.h"
#include "wbox_test_init.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "relqop.h"
#include "is_proc_alive.h"
#include "mmseg.h"
#include "format_targ_key.h"
#include "gds_map_moved.h"
#include "wcs_recover.h"
#include "wcs_sleep.h"
#include "wcs_mm_recover.h"
#include "add_inter.h"
#include "gtm_malloc.h"		/* for verifyAllocatedStorage() prototype */
#include "cert_blk.h"
#include "shmpool.h"
#include "wcs_phase2_commit_wait.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "memcoherency.h"
#include "gtm_c_stack_trace.h"
#include "wcs_wt.h"
#include "recover_truncate.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs_ptr_t */

GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			gtmDebugLevel;
GBLREF	uint4			process_id;
GBLREF	unsigned int		cr_array_index;
GBLREF	volatile boolean_t	in_wcs_recover;	/* TRUE if in "wcs_recover" */
GBLREF 	jnl_gbls_t		jgbl;
GBLREF 	sgmnt_data_ptr_t 	cs_data;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
#ifdef DEBUG
GBLREF	unsigned int		t_tries;
GBLREF	int			process_exiting;
GBLREF	boolean_t		in_mu_rndwn_file;
GBLREF	boolean_t		dse_running;
#endif

error_def(ERR_BUFRDTIMEOUT);
error_def(ERR_DBADDRALIGN);
error_def(ERR_DBADDRANGE);
error_def(ERR_DBCNTRLERR);
error_def(ERR_DBCRERR);
error_def(ERR_DBDANGER);
error_def(ERR_DBFILERR);
error_def(ERR_GBLOFLOW);
error_def(ERR_GVIS);
error_def(ERR_INVALIDRIP);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

#define	BT_NOT_NULL(BT, CR, CSA, REG)								\
{												\
	if (NULL == BT)										\
	{	/* NULL value is only possible if wcs_get_space in bt_put fails */		\
		send_msg_csa(CSA_ARG(CSA) VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(REG),	\
			CR, CR->blk, RTS_ERROR_TEXT("Non-zero bt"), BT, TRUE, CALLFROM);	\
		assert(FALSE);									\
		continue;									\
	}											\
}

void wcs_recover(gd_region *reg)
{
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr, cr_alt, cr_lo, cr_hi, hash_hdr, cr_old, cr_new;
	cache_que_head_ptr_t	active_head, hq, wip_head, wq;
	gd_region		*save_reg;
	jnlpool_addrs_ptr_t	save_jnlpool;
	que_ent_ptr_t		back_link; /* should be crit & not need interlocked ops. */
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	int4			dummy_errno, blk_size;
	uint4			jnl_status, epid, r_epid;
	int4			bt_buckets;
	inctn_opcode_t		save_inctn_opcode;
	unsigned int		bplmap, lcnt, total_rip_wait;
	sm_uc_ptr_t		buffptr;
	blk_hdr_ptr_t		blk_ptr, cr_buff, cr_alt_buff;
	INTPTR_T		bp_lo, bp_top;
	boolean_t		asyncio, twinning_on, wcs_wtfini_ret;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	sgm_info		*si;
#	ifdef DEBUG
	blk_hdr_ptr_t		cr_old_buff, cr_new_buff;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If this is the source server, do not invoke cache recovery as that implies touching the database file header
	 * (incrementing the curr_tn etc.) and touching the journal file (writing INCTN records) both of which are better
	 * avoided by the source server; It is best to keep it as read-only to the db/jnl as possible. It is ok to do
	 * so because the source server anyways does not rely on the integrity of the database cache and so does not need
	 * to fix it right away. Any other process that does rely on the cache integrity will fix it when it gets crit next.
	 */
	if (is_src_server)
		return;
	save_reg = gv_cur_region;	/* protect against [at least] M LOCK code which doesn't maintain cs_addrs and cs_data */
	save_jnlpool = jnlpool;
	TP_CHANGE_REG(reg);		/* which are needed by called routines such as wcs_wtstart and wcs_mm_recover */
	if (dba_mm == reg->dyn.addr->acc_meth)	 /* MM uses wcs_recover to remap the database in case of a file extension */
	{
		wcs_mm_recover(reg);
		TP_CHANGE_REG(save_reg);
		jnlpool = save_jnlpool;
		TREF(wcs_recover_done) = TRUE;
		return;
	}
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	si = csa->sgm_info_ptr;
	/* If a mupip truncate operation was abruptly interrupted we have to correct any inconsistencies */
	recover_truncate(csa, csd, gv_cur_region);
	/* We are going to UNPIN (reset in_cw_set to 0) ALL cache-records so assert that we are not in the middle of a
	 * non-TP or TP transaction that has already PINNED a few buffers as otherwise we will create an out-of-design state.
	 * The only exception is if we are in the 2nd phase of KILL in a TP transaction. In this case si->cr_aray_index
	 * could be non-zero as it is reset only in tp_clean_up which is invoked AFTER freeing up ALL the blocks in
	 * the function gvcst_expand_free_subtree. Work around this by checking for si->kip_csa to be NON NULL in this case.
	 */
	assert((!dollar_tlevel && !cr_array_index) || (dollar_tlevel && (!si->cr_array_index || (NULL != si->kip_csa))));
	/* We should never invoke wcs_recover in the final retry as that could cause the transaction in progress to restart
	 * (which is an out-of-design situation). There are a few exceptions e.g. tp_restart/t_retry where we have not started
	 * the transaction so allow those. Such places set the variable ok_to_call_wcs_recover to TRUE. Also if we are in
	 * the process of exiting, we are guaranteed no transaction is in progress so it is ok to invoke wcs_recover
	 * even if the variable ok_to_call_wcs_recover is not set to TRUE.
	 */
	assert((CDB_STAGNATE > t_tries) || TREF(ok_to_call_wcs_recover) || process_exiting);
	assert(csa->now_crit || csd->clustered);
	CHECK_TN(csa, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
	SIGNAL_WRITERS_TO_STOP(cnl);		/* to stop all active writers */
	WAIT_FOR_WRITERS_TO_STOP(cnl, lcnt, MAXWTSTARTWAIT);
	/* if the wait loop above hits the limit, or cnl->intent_wtstart goes negative, it is ok to proceed since
	 * wcs_verify (invoked below) reports and clears cnl->intent_wtstart and cnl->in_wtstart.
	 */
	assert(!TREF(donot_write_inctn_in_wcs_recover) || in_mu_rndwn_file || jgbl.onlnrlbk || jgbl.mur_extract);
	assert(!in_mu_rndwn_file || (0 == cnl->wcs_phase2_commit_pidcnt));
	assert(!csa->wcs_pidcnt_incremented); /* we should never have come here with a phase2 commit pending for ourself */
	/* A non-zero value of cnl->wtfini_in_prog implies a process in
	 * wcs_wtfini() was abnormally terminated (e.g. kill -9). Since we have
	 * crit here and are about to reinitialize the cache structures, it is
	 * safe to reset it here
	 */
	if (0 != cnl->wtfini_in_prog)
		cnl->wtfini_in_prog = 0;
	/* Wait for any pending phase2 commits to finish */
	if (cnl->wcs_phase2_commit_pidcnt)
	{
		wcs_phase2_commit_wait(csa, NULL); /* not checking return value since even if it fails we want to do recovery */
		/* At this point we expect cnl->wcs_phase2_commit_pidcnt to be 0. But it is possible in case of crash tests that
		 * it is non-zero (e.g. in VMS, if the only GT.M process accessing the db was STOP/IDed while in the
		 * DECR_WCS_PHASE2_COMMIT_PIDCNT macro just after resetting csa->wcs_pidcnt_incremented to FALSE but just BEFORE
		 * decrementing cnl->wcs_phase2_commit_pidcnt). Anyways wcs_verify reports and clears this field so no problems.
		 */
	}
	if (JNL_ENABLED(csd))
	{
		jpc = csa->jnl;
		assert(NULL != jpc);
		jbp = jpc->jnl_buff;
		assert(NULL != jbp);
		/* Since we have already done a "wcs_phase2_commit_wait" above, we do not need to do a
		 * "jnl_phase2_commit_wait" call separately here. But we might need to do a "jnl_phase2_cleanup"
		 * in case there are orphaned phase2 jnl writes still lying around. Take this opportunity to do that.
		 */
		jnl_phase2_cleanup(csa, jbp);
	}
	asyncio = csd->asyncio;
	twinning_on = TWINNING_ON(csd);
	wip_head = &csa->acc_meth.bg.cache_state->cacheq_wip;
	BG_TRACE_PRO_ANY(csa, wc_blocked_wcs_recover_invoked);
	if (wcs_verify(reg, TRUE, TRUE))	/* expect_damage is TRUE, in_wcs_recover is TRUE */
	{	/* if it passes verify, then recover can't help ??? what to do */
		BG_TRACE_PRO_ANY(csa, wc_blocked_wcs_verify_passed);
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBCNTRLERR, 2, DB_LEN_STR(reg));
	}
	if (gtmDebugLevel)
		verifyAllocatedStorage();
	/* Before recovering the cache, set early_tn to curr_tn + 1 to indicate to have_crit that we are in a situation that
	 * is equivalent to being in the midst of a database commit and therefore defer exit handling in case of a MUPIP STOP.
	 * wc_blocked is anyways set to TRUE at this point so the next process to grab crit will anyways attempt another recovery.
	 */
	if (!TREF(donot_write_inctn_in_wcs_recover))
	{
		csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
		/* Take this opportunity to set "cnl->last_wcs_recover_tn" BEFORE cache recover (e.g. "bt_refresh") starts.
		 * This is relied upon by the out-of-crit validation logic in tp_hist/t_end.
		 */
		cnl->last_wcs_recover_tn = csd->trans_hist.curr_tn;
	}
	assert(!in_wcs_recover);	/* should not be called if we are already in "wcs_recover" for another region */
	in_wcs_recover = TRUE;	/* used by bt_put() called below */
	bt_refresh(csa, TRUE);	/* this resets all bt->cache_index links to CR_NOTVALID */
	/* the following queue head initializations depend on the wc_blocked mechanism for protection from wcs_wtstart */
	memset(wip_head, 0, SIZEOF(cache_que_head));
	active_head = &csa->acc_meth.bg.cache_state->cacheq_active;
	memset(active_head, 0, SIZEOF(cache_que_head));
	cnl->cur_lru_cache_rec_off = GDS_ABS2REL(csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets);
	cnl->wcs_active_lvl = 0;
	cnl->wcs_wip_lvl = 0;
	cnl->wc_in_free = 0;
	bplmap = csd->bplmap;
	hash_hdr = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array;
	bt_buckets = csd->bt_buckets;
	for (cr = hash_hdr, cr_hi = cr + bt_buckets; cr < cr_hi; cr++)
		cr->blkque.fl = cr->blkque.bl = 0;	/* take no chances that the blkques are messed up */
	cr_lo = cr_hi;
	cr_hi = cr_lo + csd->n_bts;
	blk_size = csd->blk_size;
	buffptr = (sm_uc_ptr_t)ROUND_UP((sm_ulong_t)cr_hi, OS_PAGE_SIZE);
	/* After recovering the cache, we normally increment the db curr_tn. But this should not be done if
	 * 	a) Caller is forward journal recovery, since we want the final database transaction number to match
	 *		the journal file's eov_tn (to avoid JNLDBTNNOMATCH errors) OR
	 *	b) TREF(donot_write_inctn_in_wcs_recover) is TRUE.
	 * Therefore in this case, make sure all "tn" fields in the bt and cache are set to one less than the final db tn.
	 * This is done by decrementing the database current transaction number at the start of the recovery and incrementing
	 * it at the end. To keep early_tn and curr_tn in sync, decrement early_tn as well.
	 */
	if (TREF(donot_write_inctn_in_wcs_recover) || (mupip_jnl_recover && !JNL_ENABLED(csd)))
	{
		csd->trans_hist.curr_tn--;
		csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
		/* Adjust "cnl->last_wcs_recover_tn" to be in sync with adjusted curr_tn */
		cnl->last_wcs_recover_tn = csd->trans_hist.curr_tn;
	}
	for (cr = cr_lo, total_rip_wait = 0; cr < cr_hi; cr++, buffptr += blk_size)
	{
		cr->buffaddr = GDS_ANY_ABS2REL(csa, buffptr);	/* reset it to what it should be just to be safe */
		if (((int)(cr->blk) != CR_BLKEMPTY) && (((int)(cr->blk) < 0) || ((int)(cr->blk) >= csd->trans_hist.total_blks)))
		{	/* bad block number. discard buffer for now. actually can do better by looking at cr->bt_index... */
			cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
			cr->blk = CR_BLKEMPTY;
			assert(FALSE);
			SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
		}
		/* fix bad values of cr->dirty and cr->flushed_dirty_tn */
		assert(csa->ti == &csd->trans_hist);
		if (cr->dirty > csd->trans_hist.curr_tn)
			cr->dirty = csd->trans_hist.curr_tn;	/* we assume csd->trans_hist.curr_tn is valid */
		if (cr->flushed_dirty_tn >= cr->dirty)
			cr->flushed_dirty_tn = 0;
		/* wait for read-in-progress to complete */
		for (lcnt = 1; (-1 != cr->read_in_progress); lcnt++)
		{	/* very similar code appears elsewhere and perhaps should be common */
			/* Since cr->r_epid can be changing concurrently, take a local copy before using it below,
			 * particularly before calling is_proc_alive as we don't want to call it with a 0 r_epid.
			 */
			r_epid = cr->r_epid;
			if (cr->read_in_progress < -1)
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_INVALIDRIP, 2, DB_LEN_STR(reg));
				INTERLOCK_INIT(cr);
				cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
				cr->blk = CR_BLKEMPTY;
				SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
				assert(cr->r_epid == 0);
				assert(0 == cr->dirty);
			} else if ((0 != r_epid) && ((r_epid == process_id) || (FALSE == is_proc_alive(r_epid, 0))))
			{
				INTERLOCK_INIT(cr);	/* Process gone, release that process's lock */
				cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
				cr->blk = CR_BLKEMPTY;
				SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
			} else
			{
				if (1 == lcnt)
					epid = r_epid;
				else if ((BUF_OWNER_STUCK < lcnt) || (MAX_WAIT_FOR_RIP <= total_rip_wait))
				{	/* If we have already waited for atleast 4 minutes, no longer wait but fixup
					 * all following cr's. If r_epid is 0 and also read in progress, we identify
					 * this as corruption and fixup up this cr and proceed to the next cr.
					 */
					assert(WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number);
					if (!((0 == r_epid) || (epid == r_epid)))
					{
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_INVALIDRIP, 2, DB_LEN_STR(reg));
						assert(FALSE);
					}
					/* process still active but not playing fair or cache is corrupted */
					GET_C_STACK_FROM_SCRIPT("BUFRDTIMEOUT", process_id, r_epid, TWICE);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_BUFRDTIMEOUT, 6, process_id, cr->blk, cr,
						r_epid, DB_LEN_STR(reg), ERR_TEXT, 2, LEN_AND_LIT("Buffer forcibly seized"));
					INTERLOCK_INIT(cr);
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
					if (BUF_OWNER_STUCK < lcnt)
						total_rip_wait++;
					break;
				}
				DEBUG_ONLY(
				else if (((BUF_OWNER_STUCK / 2) == lcnt) || ((MAX_WAIT_FOR_RIP / 2) == total_rip_wait))
						GET_C_STACK_FROM_SCRIPT("BUFRDTIMEOUT", process_id, r_epid, ONCE);)
				wcs_sleep(lcnt);
			}
		}
		/* reset cr->rip_latch. it is unused in VMS, but wcs_verify() checks it hence the reset in both Unix and VMS */
		SET_LATCH_GLOBAL(&(cr->rip_latch), LOCK_AVAILABLE);
		cr->r_epid = 0;		/* the processing above should make this appropriate */
		cr->blkque.fl = cr->blkque.bl = 0;		/* take no chances that the blkques are messed up */
		cr->state_que.fl = cr->state_que.bl = 0;	/* take no chances that the state_ques are messed up */
		cr->in_cw_set = 0;	/* this has crit and is here, so in_cw_set must no longer be non-zero */
		/* If asyncio is TRUE and cr->epid is non-zero, it is a WIP record. Do not touch it */
		if (!asyncio)
			cr->epid = 0;
		if (0 != cr->twin)
		{
			cr->twin = 0; /* Clean up "twin" link. We will set it afresh further down below */
			cr->backup_cr_is_twin = FALSE;
		}
		if (JNL_ENABLED(csd) && cr->dirty)
		{
			if (cr->jnl_addr > jbp->rsrv_freeaddr)
				cr->jnl_addr = jbp->rsrv_freeaddr;
		} else
			cr->jnl_addr = 0;	/* just be safe */
		if (cr->data_invalid)
		{	/* Some process was shot (kill -9 in Unix) in the middle of an update. So send a DBDANGER warning first.
			 * If the buffer was dirty at the start of this current update we cannot discard this buffer (since it
			 * contains updates corresponding to prior transactions) but otherwise we can discard it as a clean
			 * copy of this block (minus the current update) can be later fetched from disk.
			 * If cr->tn is 0, it means this buffer was not dirty at the start of this current update. And vice versa.
			 * So that provides a convenient way to decide.
			 */
			if (!jgbl.mur_rollback)
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBDANGER, 5, cr->data_invalid, cr->data_invalid,
						DB_LEN_STR(reg), cr->blk);
			assert(gtm_white_box_test_case_enabled
				&& (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number));
			cr->data_invalid = 0;
			if (!cr->tn)
				cr->dirty = 0;
		}
		cr->tn = csd->trans_hist.curr_tn;
		assert(!cr->stopped || (CR_BLKEMPTY != cr->blk));
		assert(!cr->stopped || (0 == cr->twin));
		if (cr->stopped && (CR_BLKEMPTY != cr->blk))
		{	/* cache record attached to a buffer built by secshr_db_clnup: finish work; clearest case: do it 1st */
			assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr));
			if (!cert_blk(reg, cr->blk, (blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr), 0, RTS_ERROR_ON_CERT_FAIL, NULL))
			{	/* always check the block and return - no assertpro, so last argument is FALSE */
				if (!jgbl.mur_rollback)
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBDANGER, 5, cr->stopped, cr->stopped,
						DB_LEN_STR(reg), cr->blk);
				/* The only tests that we know which can produce a DBDANGER are the crash tests (which have
				 * WBTEST_CRASH_SHUTDOWN_EXPECTED white-box enabled. But there is one exception, the v62000/gtm6348
				 * subtest, which uses WBTEST_BG_UPDATE_BTPUTNULL & dse to trigger a DBDANGER. So allow that
				 * additional condition here. Other places where DBDANGER is issued should not have this ||.
				 */
				assert(gtm_white_box_test_case_enabled
					&& ((WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number)
						|| (dse_running
							&& (WBTEST_BG_UPDATE_BTPUTNULL == gtm_white_box_test_case_number))));
			}
			bt = bt_put(reg, cr->blk);
			BT_NOT_NULL(bt, cr, csa, reg);
			bt->killtn = csd->trans_hist.curr_tn;	/* be safe; don't know when was last kill after recover */
			if (CR_NOTVALID != bt->cache_index)
			{	/* the bt already identifies another cache entry with this block */
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
#				ifdef DEBUG
				cr_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr));
				cr_alt_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr));
				assert(cr_buff->tn >= cr_alt_buff->tn);
				assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
#				endif
				cr_alt->bt_index = 0;				/* cr is more recent */
				assert((LATCH_CLEAR <= WRITE_LATCH_VAL(cr_alt)) && (LATCH_CONFLICT >= WRITE_LATCH_VAL(cr_alt)));
				if (LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt))
				{	/* the previous entry is of interest to some process and therefore must be WIP:
					 * twin and make this (cr->stopped) cache record the active one.
					 */
					assert(0 == cr_alt->twin);
					cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
					cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					WRITE_LATCH_VAL(cr_alt) = LATCH_CONFLICT;	/* semaphore state of a wip twin */
				} else
				{	/* the other copy is less recent and not WIP, so discard it */
					if ((cr_alt < cr) && cr_alt->state_que.fl)
					{	/* cr_alt has already been processed and is in the state_que. hence remove it */
						wq = (cache_que_head_ptr_t)((sm_uc_ptr_t)&cr_alt->state_que + cr_alt->state_que.fl);
						assert(0 == (((UINTPTR_T)wq) % SIZEOF(cr_alt->state_que.fl)));
						assert((UINTPTR_T)wq + wq->bl == (UINTPTR_T)&cr_alt->state_que);
						back_link = (que_ent_ptr_t)remqt((que_ent_ptr_t)wq);
						assert(EMPTY_QUEUE != back_link);
						SUB_ENT_FROM_ACTIVE_QUE_CNT(cnl);
						assert(0 <= cnl->wcs_active_lvl);
						assert(back_link == (que_ent *)&cr_alt->state_que);
						/* Now that back_link is out of the active queue, reset its links to 0.
						 * The queue operation functions (see gtm_relqueopi.c) and Unix wcs_get_space
						 * rely on this to determine if an element is IN the queue or not.
						 */
						back_link->fl = 0;
						back_link->bl = 0;
					}
					cr->twin = cr_alt->twin;		/* existing cache record may have a twin */
					cr_alt->cycle++; /* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr_alt->blk = CR_BLKEMPTY;
					cr_alt->dirty = 0;
					cr_alt->flushed_dirty_tn = 0;
					cr_alt->in_tend = 0;
					SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr_alt);
					WRITE_LATCH_VAL(cr_alt) = LATCH_CLEAR;
					cr_alt->jnl_addr = 0;
					cr_alt->refer = FALSE;
					cr_alt->twin = 0;
					cr_alt->backup_cr_is_twin = FALSE;
					ADD_ENT_TO_FREE_QUE_CNT(cnl);
					if (0 != cr->twin)
					{	/* inherited a WIP twin from cr_alt, transfer the twin's affections */
						cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
#						ifdef DEBUG
						cr_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr));
						cr_alt_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr));
						assert(cr_buff->tn > cr_alt_buff->tn);
						assert(LATCH_CONFLICT == WRITE_LATCH_VAL(cr_alt)); /* semaphore for wip twin */
						assert(0 == cr_alt->bt_index);
#						endif
						cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					}
				}	/* if (LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt)) */
			}	/* if (CR_NOTVALID == cr_alt) */
			bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
			cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
			cr->dirty = csd->trans_hist.curr_tn;
			cr->flushed_dirty_tn = 0;	/* need to be less than cr->dirty. we choose 0. */
			cr->epid = 0;
			cr->in_tend = 0;
			cr->data_invalid = 0;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			cr->refer = TRUE;
			cr->stopped = 0;
			hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
			insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
			insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
			ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
			continue;
		}
		if (cr->in_tend)
		{	/* Some process was shot (kill -9 in Unix) in the middle of an update.
			 * We cannot discard this buffer so send a warning to the user and proceed.
			 */
			if (!jgbl.mur_rollback)
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBDANGER, 5, cr->in_tend, cr->in_tend,
						DB_LEN_STR(reg), cr->blk);
			assert(gtm_white_box_test_case_enabled
				&& (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number));
			cr->in_tend= 0;
			blk_ptr = (blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
			if (!blk_ptr->bsiz)
			{	/* A new twin was created with an empty buffer and process got shot before it could do
				 * a gvcst_blk_build. A 0-byte block presents problems with asyncio (since wcs_wtfini
				 * expects a non-zero return value for # of bytes written by the aio call) so
				 * just discard this buffer.
				 */
				cr->dirty = 0;
			}
		}
		if ((CR_BLKEMPTY == cr->blk) || (0 == cr->dirty))
		{	/* cache record has no valid buffer attached, or its contents are in the database */
			cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
			cr->blk = CR_BLKEMPTY;
			cr->bt_index = 0;
			cr->data_invalid = 0;
			cr->dirty = 0;
			cr->flushed_dirty_tn = 0;
			cr->epid = 0;
			cr->in_tend = 0;
			SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			cr->jnl_addr = 0;
			cr->refer = FALSE;
			cr->stopped = 0;	/* reset cr->stopped just in case it has a corrupt value */
			ADD_ENT_TO_FREE_QUE_CNT(cnl);
			continue;
		}
		if (LATCH_SET > WRITE_LATCH_VAL(cr))
		{	/* No process has an interest */
			bt = bt_put(reg, cr->blk);
			BT_NOT_NULL(bt, cr, csa, reg);
			bt->killtn = csd->trans_hist.curr_tn;	/* be safe; don't know when was last kill after recover */
			if (CR_NOTVALID == bt->cache_index)
			{	/* no previous entry for this block */
				bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
				cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
				cr->refer = TRUE;
				hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
				insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
				insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
				ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
			} else
			{	/* The bt already has an entry for the block */
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
				cr_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr));
				cr_alt_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr));
				assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
				if ((LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt)) || (cr_buff->tn > cr_alt_buff->tn))
				{	/* The previous cache record is WIP or not-WIP (possible if process in "wcs_wtstart" got
					 * killed older twin write was issued), but the current cache record is the more
					 * recent twin.
					 */
					cr_alt->bt_index = 0;
					if (LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt))
						WRITE_LATCH_VAL(cr_alt) = LATCH_CONFLICT;
					cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
					bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
					cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
					cr->refer = TRUE;
					hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
					insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
					insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
					ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
				} else
				{	/* Previous cache record is more recent from a cr->stopped record
					 * made by secshr_db_clnup. Discard this copy as it is old.
					 */
					assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr_alt));
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					cr->bt_index = 0;
					cr->dirty = 0;
					cr->flushed_dirty_tn = 0;
					cr->jnl_addr = 0;
					cr->refer = FALSE;
					SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
					ADD_ENT_TO_FREE_QUE_CNT(cnl);
				}
			}
			cr->epid = 0;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			continue;
		}
		/* interlock.semaphore is not LATCH_CLEAR so cache record must be WIP in case of no-twinning
		 * and could be in WIP or Active queue in case of twinning.
		 */
		assert(LATCH_CONFLICT >= WRITE_LATCH_VAL(cr));
		hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
		bt = bt_put(reg, cr->blk);
		BT_NOT_NULL(bt, cr, csa, reg);
		bt->killtn = csd->trans_hist.curr_tn;	/* be safe; don't know when was last kill after recover */
		if (CR_NOTVALID == bt->cache_index)
		{	/* no previous entry for this block */
			bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
			cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
			cr->refer = TRUE;
			insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
		} else
		{	/* Whichever cache record (previous or current one) has cr->epid non-zero is the WIP one.
			 * The other is the more recent one.
			 */
			assert(asyncio);
			assert(twinning_on);
			cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
			/* These two crs should be twins so one of them should have epid set. Only exception is a kill -9
			 * in a small window in wcs_wtstart after LOCK_BUFF_FOR_WRITE but before csr->epid = process_id.
			 * In that case, we will not know accurately which is the newer twin (and could even end up adding
			 * both crs into active queue for the same block) but since kill -9 is not officially supported
			 * and this is a rare scenario even within that, it is considered okay for now.
			 */
			assert(cr_alt->epid || cr->epid || (gtm_white_box_test_case_enabled
					&& (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number)));
			if (cr_alt->epid)
			{
				cr_old = cr_alt;
				cr_new = cr;
			} else
			{
				cr_old = cr;
				cr_new = cr_alt;
			}
#			ifdef DEBUG
			cr_old_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_old->buffaddr));
			cr_new_buff = ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_new->buffaddr));
			assert(cr_old_buff->tn < cr_new_buff->tn);
			assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
#			endif
			cr_old->bt_index = 0;
			cr_new->bt_index = GDS_ANY_ABS2REL(csa, bt);
			cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
			cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
			cr->refer = FALSE;
			insqt((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
		}
		if (asyncio && cr->epid && cr->aio_issued)
		{	/* WIP queue is functional AND a process had issued an aio write. Insert cr in WIP queue */
			WRITE_LATCH_VAL(cr) = LATCH_CONFLICT;
			insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)wip_head);
			ADD_ENT_TO_WIP_QUE_CNT(cnl);
		} else
		{	/* WIP queue does not exist OR a process had removed it from active queue but got kill -9'ed before
			 * setting "cr->epid" OR process had readied for write but got kill -9'ed before it issued the aio write.
			 * Use Active queue in either case.
			 */
			cr->epid = 0;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
			ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
		}
		/* end of processing for a single cache record */
	}	/* end of processing all cache records */
	assert(0 > GDS_CREATE_BLK_MAX);	/* the minimum block # is 0 which should be greater than the macro.
					 * this is relied upon by cnl->highest_lbm_blk_changed maintenance code
					 * in "bm_update" and "sec_shr_map_build".
					 */
	if (GDS_CREATE_BLK_MAX != cnl->highest_lbm_blk_changed)
	{
		csd->trans_hist.mm_tn++;
		if (!reg->read_only && !FROZEN_CHILLED(csa))
			fileheader_sync(reg);
	}
	assert((cnl->wcs_active_lvl + cnl->wcs_wip_lvl + cnl->wc_in_free) == csd->n_bts);
	assert(cnl->wcs_active_lvl || !active_head->fl);
	assert(!cnl->wcs_active_lvl || active_head->fl);
	assert(cnl->wcs_wip_lvl || !wip_head->fl);
	assert(!cnl->wcs_wip_lvl || wip_head->fl);
	assertpro(wcs_verify(reg, FALSE, TRUE));	/* expect_damage is FALSE, in_wcs_recover is TRUE */
	/* skip INCTN processing in case called from mu_rndwn_file().
	 * if called from mu_rndwn_file(), we have standalone access to shared memory so no need to increment db curr_tn
	 * or write inctn (since no concurrent GT.M process is present in order to restart because of this curr_tn change)
	 */
	if (!TREF(donot_write_inctn_in_wcs_recover) && JNL_ENABLED(csd))
	{
		assert(&FILE_INFO(jpc->region)->s_addrs == csa);
		if (!jgbl.dont_reset_gbl_jrec_time)
		{
			SET_GBL_JREC_TIME; /* needed for jnl_ensure_open, jnl_write_pini and jnl_write_inctn_rec */
		}
		assert(jgbl.gbl_jrec_time);
		/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order
		 * of jnl records. This needs to be done BEFORE the jnl_ensure_open as that could write
		 * journal records (if it decides to switch to a new journal file).
		 */
		ADJUST_GBL_JREC_TIME(jgbl, jbp);
		jnl_status = jnl_ensure_open(reg, csa);
		if (0 == jnl_status)
		{
			if (0 == jpc->pini_addr)
				jnl_write_pini(csa);
			save_inctn_opcode = inctn_opcode; /* in case caller does not expect inctn_opcode
											to be changed here */
			inctn_opcode = inctn_wcs_recover;
			jnl_write_inctn_rec(csa);
			inctn_opcode = save_inctn_opcode;
		} else
			jnl_file_lost(jpc, jnl_status);
	}
	INCREMENT_CURR_TN(csd);
	csa->wbuf_dqd = 0;	/* reset this so the wcs_wtstart below will work */
	SIGNAL_WRITERS_TO_RESUME(cnl);
	in_wcs_recover = FALSE;
	if (!reg->read_only && !FROZEN_CHILLED(csa))
	{
		dummy_errno = wcs_wtstart(reg, 0, NULL, NULL);
		/* Note: Just like in "db_csh_getn" (see comment there for more details), we do not rely on the call to
		 * "wcs_wtfini" below succeeding. Therefore we do not check the return value.
		 */
		if (asyncio)
		{
			DEBUG_ONLY(dbg_wtfini_lcnt = dbg_wtfini_wcs_recover);	/* used by "wcs_wtfini" */
			wcs_wtfini(reg, CHECK_IS_PROC_ALIVE_FALSE, NULL);
				/* try to free as many buffers from the wip queue if write is done */
		}
	}
	TP_CHANGE_REG(save_reg);
	jnlpool = save_jnlpool;
	TREF(wcs_recover_done) = TRUE;
	return;
}

#ifdef MM_FILE_EXT_OK
void	wcs_mm_recover(gd_region *reg)
{
	int			save_errno;
	gtm_uint64_t		mmap_sz;
	INTPTR_T		status;
	struct stat		stat_buf;
	sm_uc_ptr_t		old_db_addrs[2], mmap_retaddr;
	boolean_t		was_crit, read_only;
	unix_db_info		*udi;
	const char		*syscall;

	assert(&FILE_INFO(reg)->s_addrs == cs_addrs);
	assert(cs_addrs->hdr == cs_data);
	assert(!cs_addrs->hdr->clustered);
	assert(!cs_addrs->hold_onto_crit || cs_addrs->now_crit);
	if (!(was_crit = cs_addrs->now_crit))
		grab_crit(gv_cur_region);
	SET_TRACEABLE_VAR(cs_addrs->nl->wc_blocked, FALSE);
	assert((NULL != cs_addrs->db_addrs[0]) || process_exiting);
	if ((cs_addrs->total_blks == cs_addrs->ti->total_blks) || (NULL == cs_addrs->db_addrs[0]))
	{	/* I am the one who actually did the extension, don't need to remap again OR an munmap/mmap failed and we are in
		 * shutdown logic
		 */
		if (!was_crit)
			rel_crit(gv_cur_region);
		return;
	}
	old_db_addrs[0] = cs_addrs->db_addrs[0];
	old_db_addrs[1] = cs_addrs->db_addrs[1];
	cs_addrs->db_addrs[0] = NULL;
	syscall = MEM_UNMAP_SYSCALL;
#	ifdef _AIX
	status = shmdt(old_db_addrs[0] - BLK_ZERO_OFF(cs_data->start_vbn));
#	else
	status = (INTPTR_T)munmap((caddr_t)old_db_addrs[0], (size_t)(old_db_addrs[1] - old_db_addrs[0]));
#	endif
	if (-1 != status)
	{
		udi = FILE_INFO(gv_cur_region);
		FSTAT_FILE(udi->fd, &stat_buf, status);
		mmap_sz = stat_buf.st_size - BLK_ZERO_OFF(cs_data->start_vbn);
		CHECK_LARGEFILE_MMAP(gv_cur_region, mmap_sz); /* can issue rts_error MMFILETOOLARGE */
		/* Note that for a statsdb, gv_cur_region->read_only could be TRUE (and cs_addrs->read_write would be FALSE)
		 * even though we have write permissions to the statsdb (see "gvcst_init" and "gvcst_init_statsDB"). But in
		 * that case "cs_addrs->orig_read_write" would have the original permissions when the file got opened. So use
		 * that for the remap as we want to have read-write permissions on the remap if we had it on the first mmap.
		 */
		assert(cs_addrs->orig_read_write || !cs_addrs->read_write);
		read_only = !cs_addrs->orig_read_write;
		syscall = MEM_MAP_SYSCALL;
#		ifdef _AIX
		status = (sm_long_t)(mmap_retaddr = (sm_uc_ptr_t)shmat(udi->fd, (void *)NULL,
								(read_only ? (SHM_MAP|SHM_RDONLY) : SHM_MAP)));
		#else
		status = (sm_long_t)(mmap_retaddr = (sm_uc_ptr_t)MMAP_FD(udi->fd, mmap_sz,
										BLK_ZERO_OFF(cs_data->start_vbn), read_only));
#		endif
		GTM_WHITE_BOX_TEST(WBTEST_MEM_MAP_SYSCALL_FAIL, status, -1);
	}
	if (-1 == status)
	{
		save_errno = errno;
		WBTEST_ASSIGN_ONLY(WBTEST_MEM_MAP_SYSCALL_FAIL, save_errno, ENOMEM);
		if (!was_crit)
			rel_crit(gv_cur_region);
		assert(WBTEST_ENABLED(WBTEST_MEM_MAP_SYSCALL_FAIL));
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
				LEN_AND_STR(syscall), CALLFROM, save_errno);
	}
#	if defined(_AIX)
	mmap_retaddr = (sm_uc_ptr_t)mmap_retaddr + BLK_ZERO_OFF(cs_data->start_vbn);
#	endif
	gds_map_moved(mmap_retaddr, old_db_addrs[0], old_db_addrs[1], mmap_sz); /* updates cs_addrs->db_addrs[1] */
	cs_addrs->db_addrs[0] = mmap_retaddr;
	cs_addrs->total_blks = cs_addrs->ti->total_blks;
	if (!was_crit)
		rel_crit(gv_cur_region);
	return;
}
#else	/* !MM_FILE_EXT_OK */
void	wcs_mm_recover(gd_region *reg)
{
	unsigned char		*end, buff[MAX_ZWR_KEY_SZ];

	assert(&FILE_INFO(reg)->s_addrs == cs_addrs);
	assert(cs_addrs->hdr == cs_data);
	if (cs_addrs->now_crit && !cs_addrs->hold_onto_crit)
		rel_crit(reg);
	if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
		end = &buff[MAX_ZWR_KEY_SZ - 1];
	rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
	return;
}
#endif
