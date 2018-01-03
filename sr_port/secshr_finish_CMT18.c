/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "add_inter.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"		/* needed for jnlpool_addrs_ptr_t */
#include "gtmsource.h"		/* needed for jnlpool_addrs_ptr_t */
#include "secshr_db_clnup.h"
#include "sec_shr_blk_build.h"
#include "cert_blk.h"		/* for CERT_BLK_IF_NEEDED macro */
#include "interlock.h"
#include "relqueopi.h"		/* for INSQTI and INSQHI macros */
#include "caller_id.h"
#include "sec_shr_map_build.h"
#include "db_snapshot.h"
#include "shmpool.h"
#include "mupipbckup.h"

GBLREF	boolean_t		certify_all_blocks;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	uint4			process_id;
#ifdef DEBUG
GBLREF	boolean_t		dse_running;
GBLREF	node_local_ptr_t	locknl;
GBLREF	gd_region		*gv_cur_region;		/* for the LOCK_HIST macro in the RELEASE_BUFF_UPDATE_LOCK macro */
#endif

/* Returns 0 if success, -1 if failure */
int secshr_finish_CMT18(sgmnt_addrs *csa,
		sgmnt_data_ptr_t csd, boolean_t is_bg, struct cw_set_element_struct *cs, sm_uc_ptr_t blk_ptr, trans_num currtn)
{
	blk_hdr_ptr_t		old_block_ptr;
	block_id		blkid;
	cache_que_heads_ptr_t	cache_state;
	cache_rec_ptr_t		cr, cr_alt, start_cr;
	int4			n;
	int4			bufindx;	/* should be the same type as "csd->bt_buckets" */
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		bufstart;
	snapshot_context_ptr_t	lcl_ss_ctx;
	uint4			blk_size;
	int			numargs;
	gtm_uint64_t		argarray[SECSHR_ACCOUNTING_MAX_ARGS];
#	ifdef DEBUG
	jbuf_rsrv_struct_t	*jrs;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Assert that we never start db commit for any block until all journal records for this region have been written out */
	jrs = dollar_tlevel ? csa->sgm_info_ptr->jbuf_rsrv_ptr : TREF(nontp_jbuf_rsrv);
	assert((NULL == jrs) || !jrs->tot_jrec_len);
#	endif
	blk_size = csd->blk_size;
	blkid = cs->blk;
	if (is_bg)
	{
		cache_state = csa->acc_meth.bg.cache_state;
		start_cr = cache_state->cache_array + csd->bt_buckets;
		bufstart = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, start_cr->buffaddr);
		cr = cs->cr;
		ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)cr, csa);
		assert(blk_ptr == (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr));
		/* Before resetting cr->ondsk_blkver, ensure db_format in file header did not
		 * change in between phase1 (inside of crit) and phase2 (outside of crit).
		 * This is needed to ensure the correctness of the blks_to_upgrd counter.
		 */
		assert(currtn > csd->desired_db_format_tn);
		cr->ondsk_blkver = csd->desired_db_format;
	} else
		assert(blk_ptr == (MM_BASE_ADDR(csa) + (off_t)blk_size * blkid));
	cnl = csa->nl;
	/* Check if online backup is in progress and if there is a before-image to write.
	 * If so need to store link to it so wcs_recover can back it up later. Cannot
	 * rely on precomputed value csa->backup_in_prog since it is not initialized
	 * if (cw_depth == 0) (see t_end.c). Hence using cnl->nbb explicitly in check.
	 * However, for snapshots we can rely on csa as it is computed under
	 * if (update_trans). Use cs->blk_prior_state's free status to ensure that FREE
	 * blocks are not backed up either by secshr_db_clnup or wcs_recover.
	 */
	if ((SNAPSHOTS_IN_PROG(csa) || (BACKUP_NOT_IN_PROGRESS != cnl->nbb)) && (NULL != cs->old_block))
	{	/* If online backup is concurrently running, backup the block here */
		old_block_ptr = (blk_hdr_ptr_t)cs->old_block;
		if (is_bg)
		{
			ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)old_block_ptr, csa);
			bufindx = ((sm_uc_ptr_t)old_block_ptr - bufstart) / blk_size;
			assert(0 <= bufindx);
			assert(bufindx < csd->n_bts);
			cr_alt = &start_cr[bufindx];
			assert(!cr->stopped || (cr != cr_alt));
			assert(cr->stopped || (cr == cr_alt)
					|| (csd->asyncio && cr->backup_cr_is_twin && !cr_alt->backup_cr_is_twin));
			assert(cr_alt->blk == blkid);
		}
		/* The following check is similar to the one in BG_BACKUP_BLOCK */
		if (!WAS_FREE(cs->blk_prior_state) && (blkid >= cnl->nbb)
			&& (0 == csa->shmpool_buffer->failed)
			&& (old_block_ptr->tn < csa->shmpool_buffer->backup_tn)
			&& (old_block_ptr->tn >= csa->shmpool_buffer->inc_backup_tn))
		{
			if (is_bg)
				backup_block(csa, blkid, cr_alt, NULL);
			else
				backup_block(csa, blkid, NULL, blk_ptr);
			/* No need for us to flush the backup buffer. MUPIP BACKUP will anyways flush it at the end. */
		}
		if (SNAPSHOTS_IN_PROG(csa))
		{
			lcl_ss_ctx = SS_CTX_CAST(csa->ss_ctx);
			if (blkid < lcl_ss_ctx->total_blks)
			{
				if (is_bg)
					WRITE_SNAPSHOT_BLOCK(csa, cr_alt, NULL, blkid, lcl_ss_ctx);
				else
					WRITE_SNAPSHOT_BLOCK(csa, NULL, blk_ptr, blkid, lcl_ss_ctx);
			}
		}
	}
	if (gds_t_writemap == cs->mode)
		sec_shr_map_build(csa, (uint4*)cs->upd_addr, blk_ptr, cs, currtn);
	else if (0 != secshr_blk_full_build(dollar_tlevel, csa, csd, is_bg, cs, blk_ptr, currtn))
		return -1;	/* commit failed for this cse; move on to next cse */
	if (0 > cs->reference_cnt)
	{	/* blocks were freed up */
		assert(!dollar_tlevel);
		assert((inctn_bmp_mark_free_gtm == inctn_opcode) || (inctn_bmp_mark_free_mu_reorg == inctn_opcode)
				|| (inctn_blkmarkfree == inctn_opcode) || dse_running);
		/* Check if we are freeing a V4 format block and if so decrement the
		 * blks_to_upgrd counter. Do not do this in case MUPIP REORG UPGRADE/DOWNGRADE
		 * is marking a recycled block as free (inctn_opcode is inctn_blkmarkfree).
		 */
		if (((inctn_bmp_mark_free_gtm == inctn_opcode) || (inctn_bmp_mark_free_mu_reorg == inctn_opcode))
				&& (0 != inctn_detail.blknum_struct.blknum))
			DECR_BLKS_TO_UPGRD(csa, csd, 1);
	}
	assert(!cs->reference_cnt || (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit));
	if (csa->now_crit)
	{	/* Even though we know cs->reference_cnt is guaranteed to be 0 if we are in
		 * phase2 of commit (see above assert), we still do not want to be touching
		 * free_blocks in the file header outside of crit as it could potentially
		 * result in an incorrect value of the free_blocks counter. This is because
		 * in between the time we note down the current value of free_blocks on the
		 * right hand side of the below expression and assign the same value to the
		 * left side, it is possible that a concurrent process holding crit could
		 * have updated the free_blocks counter. In that case, our update would
		 * result in incorrect values. Hence don't touch this field if phase2.
		 */
		csd->trans_hist.free_blocks -= cs->reference_cnt;
	}
	cs->old_mode = (int4)cs->mode;
	assert(0 < cs->old_mode);
	cs->mode = gds_t_committed;	/* rolls forward Step (CMT18) */
	CERT_BLK_IF_NEEDED(certify_all_blocks, csa->region, cs, blk_ptr, ((gv_namehead *)NULL));
	cr = cs->cr;
	assert(!cr->stopped || (process_id == cr->stopped));
	if (!is_bg)
		return 0;
	assert(process_id == cr->in_tend);
	if (!cr->stopped)
	{	/* Reset cr->in_tend now that cr is uptodate. Take this opportunity to reset cr->in_cw_set and the
		 * write interlock thereby simulating exactly what bg_update_phase2 would have done. Also check cr->data_invalid.
		 */
		if (cr->data_invalid)
		{	/* Buffer is already in middle of update. Since blk builds are not redoable, db is in danger whether
			 * or not we redo the build. Since, skipping the build is guaranteed to give us integrity errors, we
			 * redo the build hoping it will have at least a 50% chance of resulting in a clean block. Make sure
			 * data_invalid flag is set until the next cache-recovery (wcs_recover will send a DBDANGER syslog
			 * message for this block to alert of potential database damage) by setting donot_reset_data_invalid.
			 */
			numargs = 0;
			SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
			SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_finish_CMT18);
			SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cs);
			SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cr);
			SECSHR_ACCOUNTING(numargs, argarray, cr->blk);
			SECSHR_ACCOUNTING(numargs, argarray, cr->data_invalid);
			secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
			assert(FALSE);
		}
		/* Release write interlock. The following code is very similar to that at the end of "bg_update_phase2" */
		DEBUG_ONLY(locknl = cnl;)	/* Avoid using gv_cur_region in the LOCK_HIST macro that is
						 * used by the RELEASE_BUFF_UPDATE_LOCK macro by setting locknl
						 */
		if (!cr->tn)
		{
			cr->jnl_addr = cs->jnl_freeaddr;
			assert(LATCH_SET == WRITE_LATCH_VAL(cr));
			/* Cache-record was not dirty BEFORE this update. Insert this in the active queue. */
			n = INSQTI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cache_state->cacheq_active);
			if (INTERLOCK_FAIL == n)
			{
				numargs = 0;
				SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
				SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_finish_CMT18);
				SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cr);
				SECSHR_ACCOUNTING(numargs, argarray, cr->blk);
				SECSHR_ACCOUNTING(numargs, argarray, n);
				SECSHR_ACCOUNTING(numargs, argarray, cache_state->cacheq_active.fl);
				SECSHR_ACCOUNTING(numargs, argarray, cache_state->cacheq_active.bl);
				secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
				assert(FALSE);
			}
			ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
		}
		RELEASE_BUFF_UPDATE_LOCK(cr, n, &cnl->db_latch);
		/* "n" holds the pre-release value so check that we did hold the
		 * lock before releasing it above.
		 */
		assert(LATCH_CONFLICT >= n);
		assert(LATCH_CLEAR < n);
		if (WRITER_BLOCKED_BY_PROC(n))
		{
			n = INSQHI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cache_state->cacheq_active);
			if (INTERLOCK_FAIL == n)
			{
				numargs = 0;
				SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
				SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_finish_CMT18);
				SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cr);
				SECSHR_ACCOUNTING(numargs, argarray, cr->blk);
				SECSHR_ACCOUNTING(numargs, argarray, n);
				SECSHR_ACCOUNTING(numargs, argarray, cache_state->cacheq_active.fl);
				SECSHR_ACCOUNTING(numargs, argarray, cache_state->cacheq_active.bl);
				secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
				assert(FALSE);
			}
		}
	} else
	{
		assert(process_id == cr->stopped);
		assert(!cr->backup_cr_is_twin);
	}
	RESET_CR_IN_TEND_AFTER_PHASE2_COMMIT(cr, csa, csd);	/* resets cr->in_tend & cr->in_cw_set
								 * (for older twin too if needed).
								 */
	return 0;
}
