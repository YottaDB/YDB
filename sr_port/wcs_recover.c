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

#include "gtm_string.h"
#include "gtm_time.h"
#include "gtmimagename.h"

#ifdef UNIX
#  include <sys/mman.h>
#  include "gtm_stat.h"
#  include <errno.h>
#  include <signal.h>
#elif defined(VMS)
#  include <fab.h>
#  include <iodef.h>
#  include <ssdef.h>
#else
#  error UNSUPPORTED PLATFORM
#endif

#include "ast.h"	/* needed for DCLAST_WCS_WTSTART macro in gdsfhead.h */
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "gdsbml.h"
#include "filestruct.h"
#include "gdscc.h"
#include "interlock.h"
#include "jnl.h"
#include "testpt.h"
#include "sleep_cnt.h"
#include "mupipbckup.h"
#include "wbox_test_init.h"

#ifdef UNIX
#  include "eintr_wrappers.h"
   GBLREF	sigset_t	blockalrm;
#endif

#include "send_msg.h"
#include "bit_set.h"
#include "bit_clear.h"
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

#ifdef GTM_TRUNCATE
#include "recover_truncate.h"
#endif

GBLREF	boolean_t		certify_all_blocks;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF 	sgmnt_data_ptr_t 	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;		/* needed in VMS for error logging in MM */
GBLREF	uint4			process_id;
GBLREF	testpt_struct		testpoint;
GBLREF  inctn_opcode_t          inctn_opcode;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			gtmDebugLevel;
GBLREF	unsigned int		cr_array_index;
GBLREF	uint4			dollar_tlevel;
GBLREF	volatile boolean_t	in_wcs_recover;	/* TRUE if in "wcs_recover" */
GBLREF	boolean_t		is_src_server;
#ifdef DEBUG
GBLREF	unsigned int		t_tries;
GBLREF	int			process_exiting;
GBLREF	boolean_t		in_mu_rndwn_file;
#endif

error_def(ERR_BUFRDTIMEOUT);
error_def(ERR_DBADDRALIGN);
error_def(ERR_DBADDRANGE);
error_def(ERR_DBCNTRLERR);
error_def(ERR_DBCRERR);
error_def(ERR_DBDANGER);
error_def(ERR_DBFILERR);
error_def(ERR_INVALIDRIP);
error_def(ERR_GBLOFLOW);
error_def(ERR_GVIS);
error_def(ERR_STOPTIMEOUT);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

void wcs_recover(gd_region *reg)
{
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr, cr_alt, cr_alt_new, cr_lo, cr_top, hash_hdr;
	cache_que_head_ptr_t	active_head, hq, wip_head, wq;
	gd_region		*save_reg;
	que_ent_ptr_t		back_link; /* should be crit & not need interlocked ops. */
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	int4			bml_full, dummy_errno, blk_size;
	uint4			jnl_status, epid, r_epid;
	int4			bt_buckets, bufindx;	/* should be the same type as "csd->bt_buckets" */
	inctn_opcode_t          save_inctn_opcode;
	unsigned int		bplmap, lcnt, total_blks, wait_in_rip;
	sm_uc_ptr_t		buffptr;
	blk_hdr_ptr_t		blk_ptr;
	INTPTR_T		bp_lo, bp_top, old_block;
	boolean_t		backup_block_saved, change_bmm;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	sgm_info		*si;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If this is the source server, do not invoke cache recovery as that implies touching the database file header
	 * (incrementing the curr_tn etc.) and touching the journal file (writing INCTN records) both of which are better
	 * avoided by the source server; It is best to keep it as read-only to the db/jnl as possible.  It is ok to do
	 * so because the source server anyways does not rely on the integrity of the database cache and so does not need
	 * to fix it right away. Any other process that does rely on the cache integrity will fix it when it gets crit next.
	 */
	if (is_src_server)
		return;
	save_reg = gv_cur_region;	/* protect against [at least] M LOCK code which doesn't maintain cs_addrs and cs_data */
	TP_CHANGE_REG(reg);		/* which are needed by called routines such as wcs_wtstart and wcs_mm_recover */
	if (dba_mm == reg->dyn.addr->acc_meth)	 /* MM uses wcs_recover to remap the database in case of a file extension */
	{
		wcs_mm_recover(reg);
		TP_CHANGE_REG(save_reg);
		TREF(wcs_recover_done) = TRUE;
		return;
	}
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	si = csa->sgm_info_ptr;
	/* If a mupip truncate operation was abruptly interrupted we have to correct any inconsistencies */
	GTM_TRUNCATE_ONLY(recover_truncate(csa, csd, gv_cur_region);)
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
	assert(!TREF(donot_write_inctn_in_wcs_recover) || in_mu_rndwn_file UNIX_ONLY(|| jgbl.onlnrlbk) || jgbl.mur_extract);
	assert(!in_mu_rndwn_file || (0 == cnl->wcs_phase2_commit_pidcnt));
	assert(!csa->wcs_pidcnt_incremented); /* we should never have come here with a phase2 commit pending for ourself */
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
	BG_TRACE_PRO_ANY(csa, wc_blocked_wcs_recover_invoked);
	if (wcs_verify(reg, TRUE, TRUE))	/* expect_damage is TRUE, in_wcs_recover is TRUE */
	{	/* if it passes verify, then recover can't help ??? what to do */
		BG_TRACE_PRO_ANY(csa, wc_blocked_wcs_verify_passed);
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBCNTRLERR, 2, DB_LEN_STR(reg));
	}
	if (gtmDebugLevel)
		verifyAllocatedStorage();
	change_bmm = FALSE;
	/* Before recovering the cache, set early_tn to curr_tn + 1 to indicate to have_crit that we are in a situation that
	 * is equivalent to being in the midst of a database commit and therefore defer exit handling in case of a MUPIP STOP.
	 * wc_blocked is anyways set to TRUE at this point so the next process to grab crit will anyways attempt another recovery.
	 */
	if (!TREF(donot_write_inctn_in_wcs_recover))
		csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
	assert(!in_wcs_recover);	/* should not be called if we are already in "wcs_recover" for another region */
	in_wcs_recover = TRUE;	/* used by bt_put() called below */
	bt_refresh(csa, TRUE);	/* this resets all bt->cache_index links to CR_NOTVALID */
	/* the following queue head initializations depend on the wc_blocked mechanism for protection from wcs_wtstart */
	wip_head = &csa->acc_meth.bg.cache_state->cacheq_wip;
	memset(wip_head, 0, SIZEOF(cache_que_head));
	active_head = &csa->acc_meth.bg.cache_state->cacheq_active;
	memset(active_head, 0, SIZEOF(cache_que_head));
	UNIX_ONLY(wip_head = active_head);	/* all inserts into wip_que in VMS should be done in active_que in UNIX */
	UNIX_ONLY(SET_LATCH_GLOBAL(&active_head->latch, LOCK_AVAILABLE));
	cnl->wcs_active_lvl = 0;
	cnl->wc_in_free = 0;
	bplmap = csd->bplmap;
	hash_hdr = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array;
	bt_buckets = csd->bt_buckets;
	for (cr = hash_hdr, cr_top = cr + bt_buckets; cr < cr_top;  cr++)
		cr->blkque.fl = cr->blkque.bl = 0;	/* take no chances that the blkques are messed up */
	cr_lo = cr_top;
	cr_top = cr_top + csd->n_bts;
	blk_size = csd->blk_size;
	buffptr = (sm_uc_ptr_t)ROUND_UP((sm_ulong_t)cr_top, OS_PAGE_SIZE);
	backup_block_saved = FALSE;
	if (BACKUP_NOT_IN_PROGRESS != cnl->nbb)
	{	/* Online backup is in progress. Check if secshr_db_clnup has created any cache-records with pointers
		 * to before-images that need to be backed up. If so take care of that first before doing any cache recovery.
		 */
		bp_lo = (INTPTR_T)buffptr;
		bp_top = bp_lo + ((gtm_uint64_t)csd->n_bts * csd->blk_size);
		for (cr = cr_lo; cr < cr_top;  cr++)
		{
			if (cr->stopped && (0 != cr->twin))
			{	/* Check if cr->twin points to a valid buffer. Only in that case, do the backup */
				old_block = (INTPTR_T)GDS_ANY_REL2ABS(csa, cr->twin);
				if (!IS_PTR_IN_RANGE(old_block, bp_lo, bp_top))
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
						cr, cr->blk, old_block, RTS_ERROR_TEXT("bkup_before_image_range"), bp_lo, bp_top);
					assert(FALSE);
					continue;
				} else if (!IS_PTR_ALIGNED(old_block, bp_lo, csd->blk_size))
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg), cr, cr->blk,
						RTS_ERROR_TEXT("bkup_before_image_align"), old_block, bp_lo, csd->blk_size);
					assert(FALSE);
					continue;
				}
				bufindx = (int4)((old_block - bp_lo) / csd->blk_size);
				assert(0 <= bufindx);
				assert(bufindx < csd->n_bts);
				cr_alt = &cr_lo[bufindx];
				assert((sm_uc_ptr_t)old_block == (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr));
				/* Do other checks to validate before-image buffer */
				if (cr_alt == cr)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
						RTS_ERROR_TEXT("bkup_before_image_cr_same"), cr_alt, FALSE, CALLFROM);
					assert(FALSE);
					continue;
				} else if (cr->blk != cr_alt->blk)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
						RTS_ERROR_TEXT("bkup_before_image_blk"), cr_alt->blk, cr->blk, CALLFROM);
					assert(FALSE);
					continue;
				} else if (!cr_alt->in_cw_set)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr_alt,
							cr_alt->blk, RTS_ERROR_TEXT("bkup_before_image_in_cw_set"),
							cr_alt->in_cw_set, TRUE, CALLFROM);
					assert(FALSE);
					continue;
				} else if (cr_alt->stopped)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr_alt,
							cr_alt->blk, RTS_ERROR_TEXT("bkup_before_image_stopped"),
							cr_alt->stopped, FALSE, CALLFROM);
					assert(FALSE);
					continue;
				}
				VMS_ONLY(
					/* At this point, it is possible cr_alt points to the older twin. In this case though, the
					 * commit should have errored out BEFORE the newer twin got built. This way we are
					 * guaranteed that the cache-record holding the proper before-image is indeed the older
					 * twin. This is asserted below.
					 */
					DEBUG_ONLY(
						cr_alt_new = (cr_alt->twin)
							? ((cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->twin)) : NULL;
					)
					assert(!cr_alt->twin || cr_alt->bt_index
						|| cr_alt_new->bt_index && cr_alt_new->in_tend && cr_alt_new->in_cw_set);
				)
				/* The following check is similar to the one in BG_BACKUP_BLOCK and the one in
				 * secshr_db_clnup (where backup_block is invoked)
				 */
				blk_ptr = (blk_hdr_ptr_t)old_block;
				if ((cr_alt->blk >= cnl->nbb)
					&& (0 == csa->shmpool_buffer->failed)
					&& (blk_ptr->tn < csa->shmpool_buffer->backup_tn)
					&& (blk_ptr->tn >= csa->shmpool_buffer->inc_backup_tn))
				{
					cr_alt->buffaddr = cr->twin;	/* reset it to what it should be just to be safe */
					backup_block(csa, cr_alt->blk, cr_alt, NULL);
					backup_block_saved = TRUE;
				}
			}
		}
	}
	/* After recovering the cache, we normally increment the db curr_tn. But this should not be done if called from
	 * forward journal recovery, since we want the final database transaction number to match the journal file's
	 * eov_tn (to avoid JNLDBTNNOMATCH errors). Therefore in this case, make sure all "tn" fields in the bt and cache are set
	 * to one less than the final db tn. This is done by decrementing the database current transaction number at the
	 * start of the recovery and incrementing it at the end. To keep early_tn and curr_tn in sync, decrement early_tn as well.
	 */
	if (!TREF(donot_write_inctn_in_wcs_recover) && mupip_jnl_recover && !JNL_ENABLED(csd))
	{
		csd->trans_hist.curr_tn--;
		csd->trans_hist.early_tn--;
		assert(csd->trans_hist.early_tn == (csd->trans_hist.curr_tn + 1));
	}
	for (cr = cr_lo, wait_in_rip = 0; cr < cr_top;  cr++, buffptr += blk_size)
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
		UNIX_ONLY(
			/* reset all fields that might be corrupt that wcs_verify() cares about */
			cr->epid = 0;
			cr->image_count = 0;	/* image_count needs to be reset before its usage below in case it is corrupt */
		)
		/* wait for read-in-progress to complete */
		for (lcnt = 1;  (-1 != cr->read_in_progress);  lcnt++)
		{	/* very similar code appears elsewhere and perhaps should be common */
			/* Since cr->r_epid can be changing concurrently, take a local copy before using it below,
			 * particularly before calling is_proc_alive as we dont want to call it with a 0 r_epid.
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
			} else  if ((0 != r_epid)
					&& ((r_epid == process_id) || (FALSE == is_proc_alive(r_epid, cr->image_count))))
			{
				INTERLOCK_INIT(cr);	/* Process gone, release that process's lock */
				cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
				cr->blk = CR_BLKEMPTY;
				SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
	   		} else
			{
				if (1 == lcnt)
					epid = r_epid;
				else  if ((BUF_OWNER_STUCK < lcnt) || (MAX_WAIT_FOR_RIP <= wait_in_rip))
				{	/* If we have already waited for atleast 4 minutes, no longer wait but fixup
					 * all following cr's.  If r_epid is 0 and also read in progress, we identify
					 * this as corruption and fixup up this cr and proceed to the next cr.
					 */
					assert(FALSE || (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number));
					if ((0 != r_epid) && (epid != r_epid))
						GTMASSERT;
					/* process still active but not playing fair or cache is corrupted */
					GET_C_STACK_FROM_SCRIPT("BUFRDTIMEOUT", process_id, r_epid, TWICE);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_BUFRDTIMEOUT, 6, process_id, cr->blk, cr, r_epid,
						DB_LEN_STR(reg));
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Buffer forcibly seized"));
					INTERLOCK_INIT(cr);
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
					if (BUF_OWNER_STUCK < lcnt)
						wait_in_rip++;
					break;
				}
				DEBUG_ONLY(
				else if (((BUF_OWNER_STUCK / 2) == lcnt) || ((MAX_WAIT_FOR_RIP / 2) == wait_in_rip))
						GET_C_STACK_FROM_SCRIPT("BUFRDTIMEOUT", process_id, r_epid, ONCE);)
    				wcs_sleep(lcnt);
			}
		}
		/* reset cr->rip_latch. it is unused in VMS, but wcs_verify() checks it hence the reset in both Unix and VMS */
		UNIX_ONLY(SET_LATCH_GLOBAL(&(cr->rip_latch), LOCK_AVAILABLE));
		VMS_ONLY(memset((sm_uc_ptr_t)&cr->rip_latch, 0, SIZEOF(global_latch_t)));
		cr->r_epid = 0;		/* the processing above should make this appropriate */
		cr->tn = csd->trans_hist.curr_tn;
		cr->blkque.fl = cr->blkque.bl = 0;		/* take no chances that the blkques are messed up */
		cr->state_que.fl = cr->state_que.bl = 0;	/* take no chances that the state_ques are messed up */
		cr->in_cw_set = 0;	/* this has crit and is here, so in_cw_set must no longer be non-zero */
		UNIX_ONLY(cr->wip_stopped = FALSE;)
		VMS_ONLY(
			if (cr->wip_stopped)
			{
				for (lcnt = 1; (0 == cr->iosb.cond) && is_proc_alive(cr->epid, cr->image_count); lcnt++)
				{
					if (1 == lcnt)
						epid = cr->epid;
					else  if (BUF_OWNER_STUCK < lcnt)
					{
						if ((0 != cr->epid) && (epid != cr->epid))
							GTMASSERT;
						if (0 != epid)
						{	/* process still active, but not playing fair */
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_STOPTIMEOUT, 3, epid,
									DB_LEN_STR(reg));
							send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
									LEN_AND_LIT("Buffer forcibly seized"));
							cr->epid = 0;
						}
						continue;
					}
					wcs_sleep(lcnt);
				}
				if (0 == cr->iosb.cond)
				{	/* if it's abandonned wip_stopped, treat it as a WRT_STRT_PNDNG */
					cr->iosb.cond = WRT_STRT_PNDNG;
					cr->epid = 0;
					cr->image_count = 0;
				}	/* otherwise the iosb.cond should suffice */
				cr->wip_stopped = FALSE;
			}
		)
		if (0 != cr->twin)
		{	/* clean up any old twins. in unix twin is unused so reset it without examining its value */
			VMS_ONLY(
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
				if (!CR_NOT_ALIGNED(cr_alt, cr_lo) && !CR_NOT_IN_RANGE(cr_alt, cr_lo, cr_top))
				{
					assert(((cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->twin)) == cr);
					assert((0 == cr->bt_index) || (0 == cr_alt->bt_index));		/* at least one zero */
					assert((0 != cr->bt_index) || (0 != cr_alt->bt_index));		/* at least one non-zero */
					cr_alt->twin = 0;
				}
			)
			cr->twin = 0;
		}
		if (JNL_ENABLED(csd) && cr->dirty)
		{
			if ((NULL != csa->jnl) && (NULL != csa->jnl->jnl_buff) && (cr->jnl_addr > csa->jnl->jnl_buff->freeaddr))
				cr->jnl_addr = csa->jnl->jnl_buff->freeaddr;
		} else
			cr->jnl_addr = 0;	/* just be safe */
		assert(!cr->stopped || (CR_BLKEMPTY != cr->blk));
		if (cr->stopped && (CR_BLKEMPTY != cr->blk))
		{	/* cache record attached to a buffer built by secshr_db_clnup: finish work; clearest case: do it 1st */
			assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr));
			if ((cr->blk / bplmap) * bplmap == cr->blk)
			{	/* it's a bitmap */
				if ((csd->trans_hist.total_blks / bplmap) * bplmap == cr->blk)
					total_blks = csd->trans_hist.total_blks - cr->blk;
				else
					total_blks = bplmap;
				bml_full = bml_find_free(0, (sm_uc_ptr_t)(GDS_ANY_REL2ABS(csa, cr->buffaddr)) + SIZEOF(blk_hdr),
						total_blks);
				if (NO_FREE_SPACE == bml_full)
				{
					bit_clear(cr->blk / bplmap, MM_ADDR(csd));
					if (cr->blk > cnl->highest_lbm_blk_changed)
						cnl->highest_lbm_blk_changed = cr->blk;
					change_bmm = TRUE;
				} else if (!(bit_set(cr->blk / bplmap, MM_ADDR(csd))))
				{
					if (cr->blk > cnl->highest_lbm_blk_changed)
						cnl->highest_lbm_blk_changed = cr->blk;
					change_bmm = TRUE;
				}
			}	/* end of bitmap processing */
			if (certify_all_blocks)
				cert_blk(reg, cr->blk, (blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr), 0, TRUE); /* GTMASSERT on error */
			bt = bt_put(reg, cr->blk);
			if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
				GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
			bt->killtn = csd->trans_hist.curr_tn;	/* be safe; don't know when was last kill after recover */
			if (CR_NOTVALID != bt->cache_index)
			{	/* the bt already identifies another cache entry with this block */
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
				assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
					>= ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
				assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
				cr_alt->bt_index = 0;				/* cr is more recent */
				assert(LATCH_CLEAR <= WRITE_LATCH_VAL(cr_alt) && LATCH_CONFLICT >= WRITE_LATCH_VAL(cr_alt));
				if (UNIX_ONLY(FALSE &&) LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt))
				{	/* the previous entry is of interest to some process and therefore must be WIP:
					 * twin and make this (cr->stopped) cache record the active one */
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
						SUB_ENT_FROM_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
						assert(0 <= cnl->wcs_active_lvl);
						assert(back_link == (que_ent *)&cr_alt->state_que);
						/* Now that back_link is out of the active queue, reset its links to 0.
						 * The queue operation functions (see gtm_relqueopi.c) and Unix wcs_get_space
						 * rely on this to determine if an element is IN the queue or not.
						 */
						back_link->fl = 0;
						back_link->bl = 0;
					}
					UNIX_ONLY(assert(!cr_alt->twin));
					UNIX_ONLY(cr_alt->twin = 0;)
					cr->twin = cr_alt->twin;		/* existing cache record may have a twin */
					cr_alt->cycle++; /* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr_alt->blk = CR_BLKEMPTY;
					cr_alt->dirty = 0;
					cr_alt->flushed_dirty_tn = 0;
					cr_alt->in_tend = 0;
					SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr_alt);
					WRITE_LATCH_VAL(cr_alt) = LATCH_CLEAR;
					VMS_ONLY(cr_alt->iosb.cond = 0;)
					cr_alt->jnl_addr = 0;
					cr_alt->refer = FALSE;
					cr_alt->twin = 0;
					cnl->wc_in_free++;
					UNIX_ONLY(assert(!cr->twin));
					if (0 != cr->twin)
					{	/* inherited a WIP twin from cr_alt, transfer the twin's affections */
						cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
						assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
							> ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
						assert(LATCH_CONFLICT == WRITE_LATCH_VAL(cr_alt)); /* semaphore for wip twin */
						assert(0 == cr_alt->bt_index);
						cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					}
				}	/* if (LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt)) */
			}	/* if (CR_NOTVALID == cr_alt) */
			bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
			cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
			cr->dirty = csd->trans_hist.curr_tn;
			cr->flushed_dirty_tn = 0;	/* need to be less than cr->dirty. we choose 0. */
			cr->epid = 0;
			cr->image_count = 0;
			cr->in_tend = 0;
			cr->data_invalid = 0;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			VMS_ONLY(assert(0 == cr->iosb.cond));
			VMS_ONLY(cr->iosb.cond = 0;)
			cr->refer = TRUE;
			cr->stopped = FALSE;
			hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
			insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
			insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
			ADD_ENT_TO_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
			continue;
		}
#if defined(DEBUG) && defined(UNIX)
		if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_DBDANGER == gtm_white_box_test_case_number))
		{
			gtm_wbox_input_test_case_count++;
			/* 50 has no special meaning. Just to trigger this somewhere in the middle once. */
			if (50 == gtm_wbox_input_test_case_count)
			{
				cr->blk = 0;
				cr->dirty = 1;
				cr->data_invalid = 1;
			}
		}
#endif
		if ((CR_BLKEMPTY == cr->blk) || (0 == cr->dirty) VMS_ONLY(|| ((0 != cr->iosb.cond) && (0 == cr->bt_index))))
		{	/* cache record has no valid buffer attached, or its contents are in the database,
			 * or it has a more recent twin so we don't even have to care how its write terminated */
			cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
			cr->blk = CR_BLKEMPTY;
			cr->bt_index = 0;
			cr->data_invalid = 0;
			cr->dirty = 0;
			cr->flushed_dirty_tn = 0;
			cr->epid = 0;
			cr->image_count = 0;
			cr->in_tend = 0;
			SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			VMS_ONLY(cr->iosb.cond = 0;)
			cr->jnl_addr = 0;
			cr->refer = FALSE;
			cr->stopped = FALSE;	/* reset cr->stopped just in case it has a corrupt value */
			cnl->wc_in_free++;
			continue;
		}
		if (cr->data_invalid)
		{	/* Some process was shot (kill -9 in Unix and STOP/ID in VMS) in the middle of an update.
			 * In VMS, the kernel extension routine secshr_db_clnup would have rebuilt the block nevertheless.
			 * In Unix, no rebuild would have been attempted since no kernel extension routine currently available.
			 * In either case, we do not want to discard this buffer so send a warning to the user and proceed.
			 */
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBDANGER, 5, cr->data_invalid, cr->data_invalid,
					DB_LEN_STR(reg), cr->blk);
			cr->data_invalid = 0;
		}
		if (cr->in_tend)
		{	/* caught by a failure while in bg_update, and less recent than a cache record created by secshr_db_clnup */
			if (UNIX_ONLY(FALSE &&) (LATCH_CONFLICT == WRITE_LATCH_VAL(cr)) VMS_ONLY( && (0 == cr->iosb.cond)))
			{	/* must be WIP, with a currently active write */
				assert(LATCH_CONFLICT >= WRITE_LATCH_VAL(cr));
				hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
				WRITE_LATCH_VAL(cr) = LATCH_SET;
				bt = bt_put(reg, cr->blk);
				if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
					GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
				bt->killtn = csd->trans_hist.curr_tn;	/* be safe; don't know when was last kill after recover */
				if (CR_NOTVALID == bt->cache_index)
				{	/* no previous entry for this block; more recent cache record will twin when processed */
					cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
					bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
					insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
				} else
				{	/* form the twin with the previous (and more recent) cache record */
					cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
					assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
						< ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
					assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
					assert(0 == cr_alt->twin);
					cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
					cr->bt_index = 0;
					insqt((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
				}
				assert(cr->epid); /* before inserting into WIP queue, ensure there is a writer process for this */
				insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)wip_head); /* this should be VMS only code */
			} else
			{	/* the [current] in_tend cache record is no longer of value and can be discarded */
				cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
				cr->blk = CR_BLKEMPTY;
				cr->bt_index = 0;
				cr->dirty = 0;
				cr->flushed_dirty_tn = 0;
				cr->epid = 0;
				cr->image_count = 0;
				SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
				WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
				VMS_ONLY(cr->iosb.cond = 0;)
				cr->jnl_addr = 0;
				cnl->wc_in_free++;
			}
			cr->in_tend = 0;
			cr->refer = FALSE;
			continue;
		}
		if ((LATCH_SET > WRITE_LATCH_VAL(cr)) VMS_ONLY(|| (WRT_STRT_PNDNG == cr->iosb.cond)))
		{	/* no process has an interest */
			bt = bt_put(reg, cr->blk);
			if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
				GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
			bt->killtn = csd->trans_hist.curr_tn;	/* be safe; don't know when was last kill after recover */
			if (CR_NOTVALID == bt->cache_index)
			{	/* no previous entry for this block */
				bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
				cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
				cr->refer = TRUE;
				hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
				insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
				insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
				ADD_ENT_TO_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
			} else
			{	/* the bt already has an entry for the block */
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
				assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
				if (UNIX_ONLY(FALSE &&) LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt))
				{	/* the previous cache record is WIP, and the current cache record is the more recent twin */
					assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
						> ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
					VMS_ONLY(assert(WRT_STRT_PNDNG != cr->iosb.cond));
					cr_alt->bt_index = 0;
					WRITE_LATCH_VAL(cr_alt) = LATCH_CONFLICT;
					cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
					bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
					cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
					cr->refer = TRUE;
					hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
					insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
					insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
					ADD_ENT_TO_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
				} else
				{	/* previous cache record is more recent from a cr->stopped record made by sechsr_db_clnup:
					 * discard this copy as it is old */
					assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
						<= ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
					assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr_alt));
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					cr->bt_index = 0;
					cr->dirty = 0;
					cr->flushed_dirty_tn = 0;
					cr->jnl_addr = 0;
					cr->refer = FALSE;
					SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
					cnl->wc_in_free++;
				}
			}
			cr->epid = 0;
			cr->image_count = 0;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			VMS_ONLY(assert(0 == cr->iosb.cond || WRT_STRT_PNDNG == cr->iosb.cond));
			VMS_ONLY(cr->iosb.cond = 0;)
			continue;
		}
		/* not in_tend and interlock.semaphore is not LATCH_CLEAR so cache record must be WIP */
		assert(LATCH_CONFLICT >= WRITE_LATCH_VAL(cr));
		VMS_ONLY(WRITE_LATCH_VAL(cr) = LATCH_SET;)
		UNIX_ONLY(WRITE_LATCH_VAL(cr) = LATCH_CLEAR;)
		hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
		bt = bt_put(reg, cr->blk);
		if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
			GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
		bt->killtn = csd->trans_hist.curr_tn;	/* be safe; don't know when was last kill after recover */
		if (CR_NOTVALID == bt->cache_index)
		{	/* no previous entry for this block */
			bt->cache_index = (int4)GDS_ANY_ABS2REL(csa, cr);
			cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
			cr->refer = TRUE;
			insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
		} else
		{	/* previous cache record must be more recent as this one is WIP */
			cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
			assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
				< ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
			assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
			VMS_ONLY(
				assert(WRT_STRT_PNDNG != cr->iosb.cond);
				assert(FALSE == cr_alt->wip_stopped);
				WRITE_LATCH_VAL(cr) = LATCH_CONFLICT;
				cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
				cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
			)
			cr->bt_index = 0;
			cr->refer = FALSE;
			insqt((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
		}
		VMS_ONLY(assert(cr->epid)); /* before inserting into WIP queue, ensure there is a writer process for this */
		insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)wip_head);
		UNIX_ONLY(ADD_ENT_TO_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock));
		/* end of processing for a single cache record */
	}	/* end of processing all cache records */
	if (change_bmm)
	{
		csd->trans_hist.mm_tn++;
		if (!reg->read_only)
			fileheader_sync(reg);
	}
	if (FALSE == wcs_verify(reg, FALSE, TRUE))	/* expect_damage is FALSE, in_wcs_recover is TRUE */
		GTMASSERT;
	/* skip INCTN processing in case called from mu_rndwn_file().
	 * if called from mu_rndwn_file(), we have standalone access to shared memory so no need to increment db curr_tn
	 * or write inctn (since no concurrent GT.M process is present in order to restart because of this curr_tn change)
	 */
	if (!TREF(donot_write_inctn_in_wcs_recover))
	{
		jpc = csa->jnl;
		if (JNL_ENABLED(csd) && (NULL != jpc) && (NULL != jpc->jnl_buff))
		{
			assert(&FILE_INFO(jpc->region)->s_addrs == csa);
			if (!jgbl.dont_reset_gbl_jrec_time)
			{
				SET_GBL_JREC_TIME; /* needed for jnl_ensure_open, jnl_put_jrt_pini and jnl_write_inctn_rec */
			}
			assert(jgbl.gbl_jrec_time);
			jbp = jpc->jnl_buff;
			/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order
			 * of jnl records. This needs to be done BEFORE the jnl_ensure_open as that could write
			 * journal records (if it decides to switch to a new journal file).
			 */
			ADJUST_GBL_JREC_TIME(jgbl, jbp);
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{
				if (0 == jpc->pini_addr)
					jnl_put_jrt_pini(csa);
				save_inctn_opcode = inctn_opcode; /* in case caller does not expect inctn_opcode
												to be changed here */
				inctn_opcode = inctn_wcs_recover;
				jnl_write_inctn_rec(csa);
				inctn_opcode = save_inctn_opcode;
			} else
				jnl_file_lost(jpc, jnl_status);
		}
		INCREMENT_CURR_TN(csd);
	}
	csa->wbuf_dqd = 0;	/* reset this so the wcs_wtstart below will work */
	SIGNAL_WRITERS_TO_RESUME(cnl);
	in_wcs_recover = FALSE;
	if (!reg->read_only)
	{
		DCLAST_WCS_WTSTART(reg, 0, dummy_errno);
		VMS_ONLY(
			wcs_wtfini(gv_cur_region);	/* try to free as many buffers from the wip queue if write is done */
		)
	}
	if (backup_block_saved)
		backup_buffer_flush(reg);
	TP_CHANGE_REG(save_reg);
	TREF(wcs_recover_done) = TRUE;
	return;
}

#ifdef MM_FILE_EXT_OK
void	wcs_mm_recover(gd_region *reg)
{
	int			save_errno;
	gtm_uint64_t		mmap_sz;
	INTPTR_T		status;
	struct stat     	stat_buf;
	sm_uc_ptr_t		old_db_addrs[2], mmap_retaddr;
	boolean_t       	was_crit, read_only;
	unix_db_info		*udi;
	const char		*syscall = "munmap()";

	VMS_ONLY(assert(FALSE));
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
	status = (INTPTR_T)munmap((caddr_t)old_db_addrs[0], (size_t)(old_db_addrs[1] - old_db_addrs[0]));
	if (-1 != status)
	{
		udi = FILE_INFO(gv_cur_region);
		FSTAT_FILE(udi->fd, &stat_buf, status);
		mmap_sz = stat_buf.st_size - BLK_ZERO_OFF(cs_data);
		CHECK_LARGEFILE_MMAP(gv_cur_region, mmap_sz); /* can issue rts_error MMFILETOOLARGE */
		read_only = gv_cur_region->read_only;
		syscall = "mmap()";
		status = (sm_long_t)(mmap_retaddr = (sm_uc_ptr_t)MMAP_FD(udi->fd, mmap_sz, BLK_ZERO_OFF(cs_data), read_only));
		GTM_WHITE_BOX_TEST(WBTEST_MMAP_SYSCALL_FAIL, status, -1);
	}
	if (-1 == status)
	{
		save_errno = errno;
		WBTEST_ASSIGN_ONLY(WBTEST_MMAP_SYSCALL_FAIL, save_errno, ENOMEM);
		if (!was_crit)
			rel_crit(gv_cur_region);
		assert(WBTEST_ENABLED(WBTEST_MMAP_SYSCALL_FAIL));
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
				LEN_AND_STR(syscall), CALLFROM, save_errno);
	}
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
