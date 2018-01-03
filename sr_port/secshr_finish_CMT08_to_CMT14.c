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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"
#include "gdsblkops.h"
#include "gdsbml.h"
#include "gdskill.h"
#include "copy.h"
#include "interlock.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "io.h"
#include "gtmsecshr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "is_proc_alive.h"
#include "aswp.h"
#include "util.h"
#include "compswap.h"
#include "mutex.h"
#include "add_inter.h"
#include "send_msg.h"	/* for send_msg prototype */
#include "secshr_db_clnup.h"
#include "memcoherency.h"
#include "shmpool.h"
#include "wbox_test_init.h"
#include "db_snapshot.h"
#include "muextr.h"
#include "mupip_reorg.h"
#include "sec_shr_blk_build.h"
#include "cert_blk.h"		/* for CERT_BLK_IF_NEEDED macro */
#include "gdsbgtr.h"

GBLREF	boolean_t		certify_all_blocks;
GBLREF	boolean_t		need_kip_incr;
GBLREF	cw_set_element		cw_set[];
GBLREF	int4			strm_index;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_addrs 		*kip_csa;
GBLREF	uint4			process_id;
GBLREF	uint4			update_trans;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		cr_array_index;
GBLREF	cache_rec_ptr_t		cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	trans_num		start_tn;
GBLREF	boolean_t		dse_running;
#ifdef DEBUG
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
#endif

error_def(ERR_WCBLOCKED);

/* Roll forward commit steps CMT08 thru CMT14 on region whose "csa" is an input parameter.
 * This function is called from "secshr_db_clnup" only if it was called with the "COMMIT_INCOMPLETE" parameter
 * by "t_commit_cleanup" so we are guaranteed that the commit logic has gone past CMT07 step. And it is called
 * once for each region participating in the commit logic. This function simulates the commit steps CMT08 thru CMT14
 * for the input region if not already executed. Otherwise, it is a no-op and returns right away.
 * Note that in a multi-region TP transaction, it is possible one region is at commit step CMT10 whereas later
 * participating regions have not even executed CMT08.
 */
void	secshr_finish_CMT08_to_CMT14(sgmnt_addrs *csa, jnlpool_addrs_ptr_t update_jnlpool)
{
	boolean_t		is_bg;
	cache_que_heads_ptr_t	cache_state;
	cache_rec_ptr_t		clru, cr_top, start_cr;
	cache_rec_ptr_t		cr;
	char			*wcblocked_ptr;
	cw_set_element		*cs, *cs_top, *first_cw_set, *next_cs;
	gv_namehead		*gvtarget;
	int			max_bts, old_mode;
	node_local_ptr_t	cnl;
	seq_num			strm_seqno;
	sgm_info		*si;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_ptr;
	srch_blk_status		*t1;
	trans_num		ctn;
	uint4			blk_size;
	unsigned int		lcnt;
	jbuf_rsrv_struct_t	*jrs;
	uint4			updTrans;
	int			numargs;
	gtm_uint64_t		argarray[SECSHR_ACCOUNTING_MAX_ARGS];
#	ifdef DEBUG
	cache_rec_ptr_t		actual_cr;
	gd_region		*repl_reg;
	jnlpool_ctl_ptr_t	jpl;
	sgmnt_addrs		*repl_csa;
	sgmnt_addrs		*tmp_csa;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	SECSHR_SET_CSD_CNL_ISBG(csa, csd, cnl, is_bg); 	/* sets csd/cnl/is_bg */
	assert((FALSE == csa->t_commit_crit) || (T_COMMIT_CRIT_PHASE0 == csa->t_commit_crit)
		|| (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit) || (T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit));
	if (!csa->now_crit)
	{
		assert(T_COMMIT_CRIT_PHASE1 != csa->t_commit_crit);
		return;	/* We are guaranteed Step CMT14 is complete in this region. Can return safely right away */
	}
	numargs = 0;
	SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
	SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_finish_CMT08_to_CMT14);
	SECSHR_ACCOUNTING(numargs, argarray, process_id);
	SECSHR_ACCOUNTING(numargs, argarray, csa->now_crit);
	SECSHR_ACCOUNTING(numargs, argarray, csa->t_commit_crit);
	SECSHR_ACCOUNTING(numargs, argarray, csd->trans_hist.early_tn);
	SECSHR_ACCOUNTING(numargs, argarray, csd->trans_hist.curr_tn);
	secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
	if (dollar_tlevel)
	{
		si = csa->sgm_info_ptr;
		first_cw_set = si->first_cw_set;
		updTrans = si->update_trans;
		numargs = 0;
		SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
		SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_finish_CMT08_to_CMT14);
		SECSHR_ACCOUNTING(numargs, argarray, dollar_tlevel);
		SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)first_cw_set);
		SECSHR_ACCOUNTING(numargs, argarray, si->cw_set_depth);
		secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
	} else
	{
		DEBUG_ONLY(si = NULL;)
		first_cw_set = (0 != cw_set_depth) ? cw_set : NULL;
		updTrans = update_trans;
		numargs = 0;
		SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
		SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_finish_CMT08_to_CMT14);
		SECSHR_ACCOUNTING(numargs, argarray, dollar_tlevel);
		SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)first_cw_set);
		SECSHR_ACCOUNTING(numargs, argarray, cw_set_depth);
		secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
	}
	assert((NULL == first_cw_set) || csa->now_crit || csa->t_commit_crit || dollar_tlevel);
	if (updTrans)
	{
		if (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit)
		{	/* In phase1 of commit, cti->free_blocks is updated as each bitmap cse is committed. But we do not
			 * know if an error in commit happened in a bitmap case after the counter was updated but before
			 * the cse->mode was set to gds_t_committed. So we restore the free_blocks counter in the region
			 * that is still in phase1 (and whose commit was interrupted) and play forward all cses (even the
			 * already committed ones) and update free_blocks counter for all those again. The below step
			 * restores the counter to what it was at the beginning of phase1 of commit.
			 */
			csd->trans_hist.free_blocks = csa->prev_free_blks;
		}
		ctn = csd->trans_hist.curr_tn;
		if (csd->trans_hist.early_tn != ctn)
		{	/* Process got an error in this region after CMT07 but before CMT14 */
			/* If Non-TP, there is only one region so Step CMT08 must have been executed in that region in order
			 * to even reach this function. And Step CMT09 is done right after that in "t_end" so both these
			 * steps are guaranteed to have been executed. So no need to simulate them.
			 * But in case of TP, it is possible this is a multi-region transaction and some regions have not
			 * executed Step CMT08 or CMT09. Both these steps are idempotent (i.e. safe to redo even if already done).
			 * So do them unconditionally in case of TP.
			 */
			assert(csd->trans_hist.early_tn == (ctn + 1));
			if (dollar_tlevel)
			{
				SET_T_COMMIT_CRIT_PHASE1(csa, cnl, ctn);	/* Step CMT08 for TP */
				if (jnl_fence_ctl.replication && REPL_ALLOWED(csa))
				{	/* Indication that this is an update to a replicated region that bumps the journal seqno.
					 * So finish CMT09. Note: In "tp_tend", the variable "supplementary" is TRUE if
					 * "jnl_fence_ctl.strm_seqno" is non-zero. We use that here since the local variable
					 * "supplementary" is not available here.
					 */
					strm_seqno = GET_STRM_SEQ60(jnl_fence_ctl.strm_seqno);
#					ifdef DEBUG
					repl_reg = update_jnlpool ? update_jnlpool->jnlpool_dummy_reg : NULL;
					repl_csa = ((NULL != repl_reg) && repl_reg->open) ? REG2CSA(repl_reg) : NULL;
					assert(!jnl_fence_ctl.strm_seqno
						|| ((INVALID_SUPPL_STRM != strm_index)
							&& (GET_STRM_INDEX(jnl_fence_ctl.strm_seqno) == strm_index)));
					assert((NULL != repl_csa) && repl_csa->now_crit);
					/* see "jnlpool_init" for relationship between critical and jpl */
					jpl = (jnlpool_ctl_ptr_t)((sm_uc_ptr_t)repl_csa->critical - JNLPOOL_CTL_SIZE);
					assert(jpl == update_jnlpool->jnlpool_ctl);
					assert(jpl->jnl_seqno == (jnl_fence_ctl.token + 1));
					assert(!jnl_fence_ctl.strm_seqno || (jpl->strm_seqno[strm_index] == (strm_seqno + 1)));
#					endif
					/* It is possible CMT09 has already been done in which case csa->hdr->reg_seqno
					 * would be equal to jnl_fence_ctl.token + 1. Therefore we want to avoid the assert
					 * inside the SET_REG_SEQNO macro which checks that reg_seqno is LESS than token + 1.
					 * Hence the SKIP_ASSERT_TRUE parameter usage below.
					 */
					SET_REG_SEQNO(csa, jnl_fence_ctl.token + 1, jnl_fence_ctl.strm_seqno,	\
						strm_index, strm_seqno + 1, SKIP_ASSERT_TRUE); /* Step CMT09 for TP */
				}
			} else
				assert(T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit);
			if (NULL != first_cw_set)
			{
				if (is_bg)
				{
					clru = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cnl->cur_lru_cache_rec_off);
					lcnt = 0;
					assert(cnl->sec_size);
					cache_state = csa->acc_meth.bg.cache_state;
					start_cr = cache_state->cache_array + csd->bt_buckets;
					max_bts = csd->n_bts;
					cr_top = start_cr + max_bts;
					if (!csa->wcs_pidcnt_incremented)
						INCR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
				}
				blk_size = csd->blk_size;
				cs = first_cw_set;
				cs_top = (dollar_tlevel ? NULL : (cs + cw_set_depth));
				for (next_cs = cs; cs_top != cs; cs = next_cs)
				{
					/* Step CMT10 start */
					if (dollar_tlevel)
					{
						next_cs = next_cs->next_cw_set;
						TRAVERSE_TO_LATEST_CSE(cs);
					} else
						next_cs = cs + 1;
					if (gds_t_committed < cs->mode)
					{
						assert(n_gds_t_op != cs->mode);
						if (n_gds_t_op > cs->mode)
						{	/* Currently there are only three possibilities and each is in NON-TP.
							 * In each case, no need to do any block update so simulate commit.
							 */
							assert(!dollar_tlevel);
							assert((gds_t_write_root == cs->mode) || (gds_t_busy2free == cs->mode)
									|| (gds_t_recycled2free == cs->mode));
							if ((gds_t_busy2free == cs->mode) || (gds_t_recycled2free == cs->mode))
							{
								assert(is_bg);
								assert(cr_array_index);
								assert(process_id == cr_array[0]->in_cw_set);
								assert(cr_array[0]->blk == cs->blk);
								/* Need to UNPIN corresponding cache-record. This needs to be
								 * done only if we hold crit as otherwise it means we have
								 * already done it in t_end.
								 */
								UNPIN_CACHE_RECORD(cr_array[0]);
							}
						} else
						{	/* Currently there are only two possibilities and both are in TP.
							 * In either case, need to simulate what tp_tend would have done which
							 * is to build a private copy right now if this is the first phase of
							 * KILL (i.e. we hold crit) as this could be needed in the 2nd phase
							 * of KILL.
							 */
							assert(dollar_tlevel);
							assert((kill_t_write == cs->mode) || (kill_t_create == cs->mode));
							if (!cs->done)
							{	/* Initialize cs->new_buff to non-NULL since "sec_shr_blk_build"
								 * expects this.
								 */
								if (NULL == cs->new_buff)
									cs->new_buff = (unsigned char *)
											get_new_free_element(si->new_buff_list);
								assert(NULL != cs->new_buff);
								blk_ptr = (sm_uc_ptr_t)cs->new_buff;
								if (0 != secshr_blk_full_build(dollar_tlevel,
												csa, csd, is_bg, cs, blk_ptr, ctn))
									continue;
								cs->done = TRUE;
								assert(NULL != cs->blk_target);
								CERT_BLK_IF_NEEDED(certify_all_blocks, csa->region,
											cs, cs->new_buff, ((gv_namehead *)NULL));
							}
						}
						cs->old_mode = (int4)cs->mode;
						assert(0 < cs->old_mode);
						cs->mode = gds_t_committed;
						continue;
					}
					old_mode = cs->old_mode;
					if (gds_t_committed == cs->mode)
					{	/* already processed */
						assert(0 < old_mode);
						if (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit)
						{
							assert(csa->now_crit);
							csd->trans_hist.free_blocks -= cs->reference_cnt;
						}
						assert(!dollar_tlevel || (gds_t_write_root != old_mode));
						assert(gds_t_committed != old_mode);
#						ifdef DEBUG
						if (is_bg)
						{
							cr = cs->cr;
							if (gds_t_committed > old_mode)
								assert(process_id != cr->in_tend);
							else
							{	/* For the kill_t_* case, cs->cr will be NULL as bg_update was not
								 * invoked and the cw-set-elements were memset to 0 in TP. But for
								 * gds_t_write_root and gds_t_busy2free, they are non-TP ONLY modes
								 * and cses are not initialized so can't check for NULL cr.
								 * "n_gds_t_op" demarcates the boundaries between non-TP only and
								 * TP only modes. So use that.
								 */
								assert((n_gds_t_op > old_mode) || (NULL == cr));
							}
							assert((NULL == cr) || (cr->ondsk_blkver == csd->desired_db_format));
						}
#						endif
						continue;
					}
					if (is_bg)
					{
						assert(T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit);
						/* A positive value of cse->old_mode implies phase1 is not complete on this cse
						 * so we need to do phase1 tasks (e.g. blks_to_upgrd counter adjustment,
						 * find a cr for cs->cr etc.
						 */
						assert((0 <= old_mode) || (old_mode == -cs->mode));
						if (0 <= old_mode)
						{	/* We did not yet finish phase1 of commit for this cs (note: we also hold
							 * crit on this region), so have to find out a free cache-record
							 * we can dump our updates onto. Also, a positive value of "old_mode"
							 * implies it's value is not necessarily cs->mode. So initialize that too.
							 */
							old_mode = cs->mode;
							for ( ; lcnt < max_bts; lcnt++)
							{	/* find any available cr */
								if (++clru >= cr_top)
									clru = start_cr;
								assert(!clru->stopped);
								if (!clru->stopped && (0 == clru->dirty)
										&& (0 == clru->in_cw_set)
										&& (!clru->in_tend)
										&& (-1 == clru->read_in_progress))
									break;
							}
							if (lcnt >= max_bts)
							{	/* Did not find space in global buffers to finish commit */
								numargs = 0;
								SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
								SECSHR_ACCOUNTING(numargs, argarray,
											sac_secshr_finish_CMT08_to_CMT14);
								SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cs);
								SECSHR_ACCOUNTING(numargs, argarray, cs->blk);
								SECSHR_ACCOUNTING(numargs, argarray, cs->tn);
								SECSHR_ACCOUNTING(numargs, argarray, cs->level);
								SECSHR_ACCOUNTING(numargs, argarray, cs->done);
								SECSHR_ACCOUNTING(numargs, argarray, cs->forward_process);
								SECSHR_ACCOUNTING(numargs, argarray, cs->first_copy);
								secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
								assert(FALSE);
								continue;
							}
							cr = clru;
							/* Set cr->in_cw_set & cr->in_tend to avoid this "cr" from being
							 * pinned by another mumps process concurrently after this "secshr_db_clnup"
							 * invocation releases crit in phase1 but before it is done phase2. In that
							 * case, the other mumps process could end up with an assertpro in the
							 * PIN_CACHE_RECORD macro in t_end/tp_tend.
							 */
							if (!dollar_tlevel)	/* stuff it in the array before setting in_cw_set */
							{
								assert(ARRAYSIZE(cr_array) > cr_array_index);
								PIN_CACHE_RECORD(cr, cr_array, cr_array_index);
							} else
								TP_PIN_CACHE_RECORD(cr, si);
							cr->backup_cr_is_twin = FALSE;
							cr->in_tend = process_id;
							cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
							assert(cs->blk < csd->trans_hist.total_blks);
							cr->blk = cs->blk;
							assert(CR_BLKEMPTY != cr->blk);
							cr->jnl_addr = cs->jnl_freeaddr;
							assert(!cr->twin);
							cr->stopped = process_id;
							/* Keep cs->cr and t1->cr up-to-date to ensure clue will be accurate */
							cs->cr = cr;
							cs->cycle = cr->cycle;
							if (!IS_BITMAP_BLK(cs->blk))
							{	/* Not a bitmap block, update clue history to reflect new cr */
								assert((0 <= cs->level) && (MAX_BT_DEPTH > cs->level));
								gvtarget = cs->blk_target;
								assert((MAX_BT_DEPTH + 1) == ARRAYSIZE(gvtarget->hist.h));
								if ((0 <= cs->level) && (MAX_BT_DEPTH > cs->level)
									&& (NULL != gvtarget) && (0 != gvtarget->clue.end))
								{
									t1 = &gvtarget->hist.h[cs->level];
									if (t1->blk_num == cs->blk)
									{
										t1->cr = cr;
										t1->cycle = cs->cycle;
										t1->buffaddr = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa,
														cr->buffaddr);
									}
								}
							}
							/* the following code is very similar to that in bg_update_phase1 */
							if (gds_t_acquired == cs->mode)
							{
								if (GDSV4 == csd->desired_db_format)
									INCR_BLKS_TO_UPGRD(csa, csd, 1);
							} else
							{
#								ifdef DEBUG
								/* We rely on the fact that cs->ondsk_blkver accurately reflects
								 * the on-disk block version of the block and therefore can be
								 * used to set cr->ondsk_blkver. Confirm this by checking that
								 * if a cr exists for this block, then that cr's ondsk_blkver
								 * matches with the cs. db_csh_get uses the global variable
								 * cs_addrs to determine the region. So make it uptodate temporarily
								 * holding its value in the local variable tmp_csa.
								 */
								tmp_csa = cs_addrs;	/* save cs_addrs in local */
								cs_addrs = csa;		/* set cs_addrs for db_csh_get */
								actual_cr = db_csh_get(cs->blk);
								cs_addrs = tmp_csa;	/* restore cs_addrs */
								/* actual_cr can be NULL if the block is NOT in the cache.
								 * It can be CR_NOTVALID if the cache record originally
								 * containing this block got reused for a different block
								 * (i.e. cr->stopped = non-zero) as part of secshr_db_clnup.
								 */
								assert((NULL == actual_cr)
									|| ((cache_rec_ptr_t)CR_NOTVALID == actual_cr)
									|| (cs->ondsk_blkver == actual_cr->ondsk_blkver));
#								endif
								cr->ondsk_blkver = cs->ondsk_blkver;
								if (cr->ondsk_blkver != csd->desired_db_format)
								{
									if (GDSV4 == csd->desired_db_format)
									{
										if (gds_t_write_recycled != cs->mode)
											INCR_BLKS_TO_UPGRD(csa, csd, 1);
									} else
									{
										if (gds_t_write_recycled != cs->mode)
											DECR_BLKS_TO_UPGRD(csa, csd, 1);
									}
								}
							}
							cs->old_mode = -old_mode;	/* signal phase1 is complete */
							assert(0 > cs->old_mode);
							blk_ptr = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
							if (gds_t_writemap == cs->mode)
							{	/* Since we picked a different "cr" for the bitmap block, take a
								 * copy of the original block (before calling "sec_shr_map_build"
								 * inside "secshr_finish_CMT18") since bitmap cses do not have a
								 * notion of "cse->first_copy".
								 */
								memmove(blk_ptr, cs->old_block, blk_size);
							}
						} else
						{	/* Phase1 commit finished for this cs and cr is chosen. Assert few things */
							cr = cs->cr;
							blk_ptr = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
						}
						assert(process_id == cr->in_tend);
						assert(process_id == cr->in_cw_set);
						assert(cr->blk == cs->cr->blk);
					} else
						blk_ptr = MM_BASE_ADDR(csa) + (off_t)blk_size * cs->blk;
					/* Step CMT10 end */
					/* If BG and DSE, it is possible cache-records corresponding to global buffers which
					 * are pointed to by the update array are not pinned (because dse passes "dummy_hist"
					 * to "t_end"). In that case, we cannot wait to finish the phase2 of commit outside
					 * of crit as the global buffer we are relying on in the update array could be reused
					 * for a different block once we release crit in phase1. Hence the " || dse_running"
					 * check below.
					 */
					assert(!dse_running || !dollar_tlevel);
					if (!is_bg || (!dollar_tlevel && ((gds_t_writemap == cs->mode) || dse_running))
						|| (dollar_tlevel && IS_BG_PHASE2_COMMIT_IN_CRIT(cs, cs->mode)))
					{	/* Below is Step 10a (comprises Steps CMT16 and CMT18 */
						if (dollar_tlevel)
							jrs = si->jbuf_rsrv_ptr;
						else
							jrs = TREF(nontp_jbuf_rsrv);
						/* Below is Step CMT16 (done as part of CMT10a) */
						if (NEED_TO_FINISH_JNL_PHASE2(jrs))
							FINISH_JNL_PHASE2_IN_JNLBUFF(csa, jrs);
						/* Below is Step CMT18 (done as part of CMT10a) */
						if (0 != secshr_finish_CMT18(csa, csd, is_bg, cs, blk_ptr, ctn))
							continue;	/* error during CMT18, move on to next cs */
					}
				}
			}
			if (dollar_tlevel)
				si->update_trans = updTrans | UPDTRNS_TCOMMIT_STARTED_MASK;	/* Step CMT11 for TP */
			else
				update_trans = updTrans | UPDTRNS_TCOMMIT_STARTED_MASK;	/* Step CMT11 for Non-TP */
			INCREMENT_CURR_TN(csd);	/* roll forward Step (CMT12) */
		}
		/* else : early_tn == curr_tn and so Step CMT12 is done */
		csa->t_commit_crit = T_COMMIT_CRIT_PHASE2;			/* Step CMT13 */
		/* Check if kill_in_prog flag in file header has to be incremented. */
		if (dollar_tlevel)
		{
			if ((NULL != si->kill_set_head) && (NULL == si->kip_csa))
				INCR_KIP(csd, csa, si->kip_csa);
			si->start_tn = ctn;	/* needed by "secshr_finish_CMT18_to_CMT19" */
		} else
		{	/* Non-TP. Check need_kip_incr and value pointed to by kip_csa. */
			/* Note that kip_csa could be NULL if we are in the
			 * 1st phase of the M-kill and NON NULL if we are in the 2nd phase of the kill.
			 * Only if it is NULL, should we increment the kill_in_prog flag.
			 */
			if (need_kip_incr && (NULL == kip_csa))
			{
				INCR_KIP(csd, csa, kip_csa);
				need_kip_incr = FALSE;
			}
			if (MUSWP_INCR_ROOT_CYCLE == TREF(in_mu_swap_root_state))
				cnl->root_search_cycle++;
			start_tn = ctn;	/* needed by "secshr_finish_CMT18_to_CMT19" */
		}
		if (is_bg)
		{	/* The cache is suspect at this point so set cnl->wc_blocked to TRUE to force a cache-recovery.
			 * Need to do this BEFORE releasing crit. If we wait until phase2 is complete to set wc_blocked, it
			 * is possible another process P2 gets crit before our (P1) phase2 is complete and wants to update the
			 * exact same cr that got a WBTEST_BG_UPDATE_DBCSHGETN_INVALID2 error in phase1 for P1. In that case,
			 * P1 would have created a cr_new (with cr_new->stopped non-zero) to dump the updates but cr->in_cw_set
			 * would still be non-zero and that means P2 will assertpro when it does the PIN_CACHE_RECORD on "cr".
			 * Setting wc_blocked by P1 before releasing crit will ensure P2 will invoke "wcs_recover" as part of
			 * its "grab_crit" in t_end/tp_tend and that would wait for P1's phase2 to finish before proceeding
			 * with cache-recovery and P2's transaction commit.
			 */
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			wcblocked_ptr = WCBLOCKED_NOW_CRIT_LIT;
			BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_now_crit);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
						LEN_AND_STR(wcblocked_ptr), process_id, &ctn, DB_LEN_STR(csa->region));
		}
	}	/* if (updTrans) */
	assert(csa->region->open);
	secshr_rel_crit(csa->region, IS_EXITING_FALSE, IS_REPL_REG_FALSE);	/* Step CMT14 */
	return;
}
