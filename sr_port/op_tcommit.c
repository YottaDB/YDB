/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_ctype.h"
#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "gdsbml.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "min_max.h"		/* needed for MM extend */
#include "gdsblkops.h"		/* needed for MM extend */

#include "tp.h"
#include "tp_frame.h"
#include "copy.h"
#include "interlock.h"
#include "gdsbgtr.h"		/* for the BG_TRACE_PRO macros */

/* Include proto-types.. */
#include "t_qread.h"
#include "tp_timeout.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "lv_val.h"		/* needed for tp_unwind.h */
#include "gvcst_expand_free_subtree.h"
#include "format_targ_key.h"
#include "bm_getfree.h"
#include "tp_unwind.h"
#include "wcs_mm_recover.h"
#include "add_inter.h"
#include "tp_incr_commit.h"
#include "have_crit.h"
#include "jobinterrupt_process.h"
#include "jnl_get_checksum.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "wbox_test_init.h"
#include "memcoherency.h"
#include "util.h"
#include "op_tcommit.h"
#include "caller_id.h"
#include "process_deferred_stale.h"
#include "wcs_timer_start.h"
#include "mupipbckup.h"
#include "gvcst_protos.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */
#include "gvt_inline.h"
#include "db_snapshot.h"
#include "jnl_file_close_timer.h"
#include "gtm_time.h"			/* for clock_gettime */

#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#include "gtm_trigger.h"
#endif

GBLREF	boolean_t		block_is_free, in_timed_tn, is_updproc, mupip_jnl_recover, tp_kill_bitmaps,
				unhandled_stale_timer_pop;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	uint4			process_id, dollar_tlevel, dollar_trestart;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info, *sgm_info_ptr;
GBLREF	tp_frame		*tp_pointer;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	unsigned char		rdfail_detail, t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int		t_tries;
GBLREF	void			(*tp_timeout_clear_ptr)(boolean_t toss_queued);
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_INVOKE_RESTART;
GBLREF	int4			gtm_trigger_depth, tstart_trigger_depth;
#endif
GBLREF	int4			tstart_gtmci_nested_level;
#ifdef DEBUG
GBLREF	boolean_t		forw_recov_lgtrig_only;
#endif

error_def(ERR_GBLOFLOW);
error_def(ERR_TLVLZERO);
#ifdef GTM_TRIGGER
error_def(ERR_TRIGTCOMMIT);
error_def(ERR_TCOMMITDISALLOW);
#endif

#define bml_wide	9	/* 2**9 = 0x200 */
#define bml_adj_span	0xF	/* up to 16 bit maps away */

STATICFNDCL void fix_updarray_and_oldblock_ptrs(sm_uc_ptr_t old_db_addrs[2], sgm_info *si);

STATICFNDEF void fix_updarray_and_oldblock_ptrs(sm_uc_ptr_t old_db_addrs[2], sgm_info *si)
{
	cw_set_element		*cse;
	srch_blk_status		*t1;
	blk_segment		*array, *seg, *stop_ptr;
	sm_long_t		delta;
	sgmnt_addrs		*csa;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	csa = si->tp_csa;
	delta = (sm_long_t)(csa->db_addrs[0] - old_db_addrs[0]);
	assert(0 != delta);
	/* update cse's update array and old_block */
	for (cse = si->first_cw_set; NULL != cse; cse = cse->next_cw_set)
	{
		TRAVERSE_TO_LATEST_CSE(cse);
		if (gds_t_writemap != cse->mode)
		{
			array = cse->upd_addr.blk;
			stop_ptr = array; /* Note: No cse->first_copy optimization done here (like in gvcst_blk_build). GTM-8523 */
			seg = (blk_segment *)array->addr;
			while (seg != stop_ptr)
			{
				if ((old_db_addrs[0] <= seg->addr) && (old_db_addrs[1] >= seg->addr))
					seg->addr += delta;
				seg--;
			}
		}
		if (NULL != cse->old_block)
		{
			if ((old_db_addrs[0] <= cse->old_block) && (old_db_addrs[1] >= cse->old_block))
				cse->old_block += delta;
			/* else, old_block is already updated -- this is mostly the case with gds_t_writemap in which case
			 * bm_getfree invokes t_write_map
			 */
#			ifdef DEBUG
			if (!((csa->db_addrs[0] <= cse->old_block) && (csa->db_addrs[1] >= cse->old_block)))
			{	/* cse->old_block is pointing outside mmap bounds; most likely it points to the private memory.
				 * But cse->old_block, at all times should point to the before image of the database block and so
				 * should NOT point to private memory. This indicates that t_qread (below) did a private build on
				 * an incorrect block and tp_tend will detect this and restart. To be sure, set donot_commit.
				 */
				assert(CDB_STAGNATE > t_tries);
				TREF(donot_commit) |= DONOTCOMMIT_T_QREAD_BAD_PVT_BUILD;
			}
#			endif
		}
	}
	/* update all the tp_hist */
	for (t1 = si->first_tp_hist; t1 != si->last_tp_hist; t1++)
	{
		if ((old_db_addrs[0] <= t1->buffaddr) && (old_db_addrs[1] >= t1->buffaddr))
			t1->buffaddr += delta;
	}
	return;
}

enum cdb_sc	op_tcommit(void)
{
	blk_hdr_ptr_t		old_block;
	block_id		new_blk;
	boolean_t		before_image_needed, blk_used, is_mm, skip_invoke_restart;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress
						    * This is used to read before-images of blocks whose cs->mode is gds_t_create
						    */
	cw_set_element		*cse = NULL, *last_cw_set_before_maps, *csetemp, *first_cse;
	enum cdb_sc		status;
	gd_region		*save_cur_region;	/* saved copy of gv_cur_region before TP_CHANGE_REG modifies it */
	int			cw_depth, old_cw_depth;
	jnlpool_addrs_ptr_t	save_jnlpool;
	jnl_buffer_ptr_t	jbp;			/* jbp is non-NULL only if before-image journaling */
	kill_set		*ks;
	node_local_ptr_t	cnl;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sgm_info		*si, *temp_si;
	sm_uc_ptr_t		old_db_addrs[2];	/* for MM extend */
	struct timespec		ts;
	tp_region		*tr;
	unsigned int		bsiz;
#	ifdef DEBUG
	enum cdb_sc		prev_status;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef GTM_TRIGGER
	DBGTRIGR((stderr, "op_tcommit: Entry from 0x"lvaddr"\n", caller_id(0)));
	skip_invoke_restart = skip_INVOKE_RESTART;	/* note down global value in local variable */
	skip_INVOKE_RESTART = FALSE;	/* reset global variable to default state as soon as possible */
#	else
	skip_invoke_restart = FALSE;	/* no triggers so set local variable to default state */
#	endif
	if (!dollar_tlevel)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TLVLZERO);
	assert(0 == jnl_fence_ctl.level);
	status = cdb_sc_normal;
	tp_kill_bitmaps = FALSE;
#	ifdef GTM_TRIGGER
	/* The value of $ztlevel at the time of the TSTART, i.e. tstart_trigger_depth, can never be GREATER than
	 * the current $ztlevel as otherwise a TPQUIT error would have been issued as part of the QUIT of the
	 * M frame of the TSTART (which has a deeper trigger depth). Assert that.
	 */
	assert(tstart_trigger_depth <= gtm_trigger_depth);
#	endif
	assert(tstart_gtmci_nested_level <= TREF(gtmci_nested_level));
	if (1 == dollar_tlevel)		/* real commit */
	{
#		ifdef GTM_TRIGGER
		if (gtm_trigger_depth != tstart_trigger_depth)
		{	/* TCOMMIT to $tlevel=0 is being attempted at a trigger depth which is NOT EQUAL TO the trigger
			 * depth at the time of the TSTART. This means we have a gvcst_put/gvcst_kill frame in the
			 * C stack that invoked us through gtm_trigger and is in an incomplete state waiting for the
			 * trigger invocation to be done before completing the explicit (outside-trigger) update.
			 * Cannot commit such a transaction. Issue error.
			 */
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TRIGTCOMMIT, 2, gtm_trigger_depth,
				tstart_trigger_depth);
		}
		if (tstart_gtmci_nested_level != TREF(gtmci_nested_level))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CALLINTCOMMIT, 2,
					TREF(gtmci_nested_level), tstart_gtmci_nested_level);
		if (tp_pointer->cannot_commit)
		{	/* If this TP transaction was implicit, any unhandled error when crossing the trigger boundary
			 * would have caused a rethrow of the error in the M frame that held the non-TP update which
			 * would then have invoked "error_return" that would in turn have rolled back the implicit TP
			 * transaction so we should never see an implicit TP inside op_tcommit.
			 */
			assert(!tp_pointer->implicit_tstart);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TCOMMITDISALLOW);
		}
#		endif
		save_cur_region = gv_cur_region;
		save_jnlpool = jnlpool;
#		ifdef DEBUG
		/* With jgbl.forw_phase_recovery, it is possible gv_currkey is non-NULL and gv_target is NULL
		 * (due to a MUR_CHANGE_REG) so do not invoke the below macro in that case.
		 */
		if (!forw_recov_lgtrig_only && (!jgbl.forw_phase_recovery || (NULL != gv_target)))
			DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
#		endif
		if (NULL != first_sgm_info)	/* if (database work in the transaction) */
		{
			for (temp_si = si = first_sgm_info; (cdb_sc_normal == status) && (NULL != si); si = si->next_sgm_info)
			{
				sgm_info_ptr = si;		/* for t_qread (at least) */
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
				csa = cs_addrs;
				csd = cs_data;
				cnl = csa->nl;
				is_mm = (dba_mm == csa->hdr->acc_meth);
				si->cr_array_index = 0;
#				ifdef DEBUG /* This code is shared by two WB tests */
				if (WBTEST_ENABLED(WBTEST_MM_CONCURRENT_FILE_EXTEND) ||
					(WBTEST_ENABLED(WBTEST_WSSTATS_PAUSE) && (10 == ydb_white_box_test_case_count)))
					if (!MEMCMP_LIT(gv_cur_region->rname, "DEFAULT") && !csa->nl->wbox_test_seq_num)
					{
						while (0 == csa->nl->wbox_test_seq_num)
						SHORT_SLEEP(1); /* wait for signal from mubfilcpy that it has reached sync point */
					}
#				endif
				if (!is_mm && (si->cr_array_size < (si->num_of_blks + (si->cw_set_depth * 2))))
				{	/* reallocate a bigger cr_array. We need atmost read-set (si->num_of_blks) +
					 * write-set (si->cw_set_depth) + bitmap-write-set (a max. of si->cw_set_depth)
					 */
					free(si->cr_array);
					si->cr_array_size = si->num_of_blks + (si->cw_set_depth * 2);
					si->cr_array = (cache_rec_ptr_ptr_t)malloc(SIZEOF(cache_rec_ptr_t) * si->cr_array_size);
				}
				assert(!is_mm || (0 == si->cr_array_size && NULL == si->cr_array));
				/* whenever si->first_cw_set is non-NULL, ensure that si->update_trans is non-zero */
				assert((NULL == si->first_cw_set) || si->update_trans);
				/* If LGTRIG only TP transaction by forward recovery, assert no db blocks */
#				ifdef DEBUG
				if (forw_recov_lgtrig_only)
				{
					jnl_format_buffer       *jfb;
					assert(NULL == si->first_cw_set);
					if (JNL_ENABLED(csa))
					{
						for (jfb = si->jnl_head; (NULL != jfb); jfb = jfb->next)
							GTMTRIG_ONLY(assert(JNL_LGTRIG == jfb->ja.operation));
					}
				}
#				endif
				/* Whenever si->first_cw_set is NULL, ensure that si->update_trans is FALSE
				 * except (1) when there are duplicate sets in which case also ensure that if the database
				 * is journaled, at least one journal record is being written or
				 * (2) when there has been a ZTRIGGER in this transaction.
				 */
				assert((NULL != si->first_cw_set) || !si->update_trans
				       || (UPDTRNS_ZTRIGGER_MASK & si->update_trans)
				       || (gvdupsetnoop && (!JNL_ENABLED(csa) || (NULL != si->jnl_head))));
				if (NULL != si->first_cw_set)
				{	/* at least one update to this region in this TP transaction */
					assert(0 != si->cw_set_depth);
					cw_depth = si->cw_set_depth;
					/* Caution : since csa->backup_in_prog is initialized below only if si->first_cw_set is
					 * non-NULL, it should be used in "tp_tend" only within an if (NULL != si->first_cw_set)
					 */
					csa->backup_in_prog = (BACKUP_NOT_IN_PROGRESS != cnl->nbb);
					jbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
					if (SNAPSHOTS_IN_PROG(cnl))
					{
						/* If snapshot context is not already created, then create one now to be used
						 * by this transaction. If context creation failed (for instance, on snapshot
						 * file open fail), then SS_INIT_IF_NEEDED sets csa->snapshot_in_prog is to
						 * FALSE.
						 */
						SS_INIT_IF_NEEDED(csa, cnl);
					}
					read_before_image = ((NULL != jbp) || csa->backup_in_prog || SNAPSHOTS_IN_PROG(csa));
					/* The following section allocates new blocks required by the transaction it is done
					 * before going crit in order to reduce the change of having to wait on a read while crit.
					 * The trade-off is that if a newly allocated block is "stolen," it will cause a restart.
					 */
					if (NULL == si->first_cw_bitmap)
					{	/* si->first_cw_bitmap can be non-NULL only if there was an rts_error()
						 * in "tp_tend" and ZTRAP handler was invoked which in turn did a TCOMMIT.
						 * in that case do not reset si->first_cw_bitmap.
						 */
						last_cw_set_before_maps = si->last_cw_set;
					} else
						last_cw_set_before_maps = NULL;
					for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
					{	/* assert to ensure we haven't missed out on resetting jnl_freeaddr for any cse
						 * in t_write/t_create/t_write_map/t_write_root/mu_write_map [D9B11-001991] */
						assert(0 == cse->jnl_freeaddr);
						TRAVERSE_TO_LATEST_CSE(cse);
						if (gds_t_create == cse->mode)
						{
							old_cw_depth = si->cw_set_depth;
							first_cse = si->first_cw_set;
							TRAVERSE_TO_LATEST_CSE(first_cse);
							old_db_addrs[0] = csa->db_addrs[0];
							old_db_addrs[1] = csa->db_addrs[1];
							/* cse->blk could be a real block or a chain; we can't use a chain but
							 * the following statement is unconditional because, in general, the region
							 * hint works at least as well as the last read block, which is what we use
							 * in non-TP. We assign the hints in op_tcommit just before we grab crit to
							 * maximize chances that the blocks we assign remain available in tp_tend.
							 */
							cse->blk = ++csa->tp_hint; /* bm_getfree increments, but +2 seems better */
							if (t_tries && (cdb_sc_bmlmod == t_fail_hist[t_tries - 1]))
							{	/* This seems like a place to try minimzing cdb_sc_blkmod; balance:
								 * adjacency, computational cost & conflict frequency; this uses
								 * time & pid for dispersion within a 16 bit map range
								 */
								clock_gettime(CLOCK_REALTIME, &ts);
								cse->blk = (cse->blk & ~(-(block_id)BLKS_PER_LMAP))
									+ (((uint4)(process_id * ts.tv_nsec) & bml_adj_span)
									<< bml_wide);
							}
							while (FILE_EXTENDED == (new_blk = bm_getfree(cse->blk, &blk_used,
								cw_depth, first_cse, &si->cw_set_depth)))
							{
								assert(is_mm);
								MM_DBFILEXT_REMAP_IF_NEEDED(csa, si->gv_cur_region);
								if (csa->db_addrs[0] != old_db_addrs[0])
								{
									fix_updarray_and_oldblock_ptrs(old_db_addrs, si);
									old_db_addrs[0] = csa->db_addrs[0];
									old_db_addrs[1] = csa->db_addrs[1];
								}
							}
							assert(csd == csa->hdr);
							if (0 > new_blk)
							{
								GET_CDB_SC_CODE(new_blk, status); /* code is set in status */
								break;	/* transaction must attempt restart */
							} else
								blk_used ? BIT_CLEAR_FREE(cse->blk_prior_state)
									 : BIT_SET_FREE(cse->blk_prior_state);
							csa->tp_hint = new_blk;		/* remember for next time in this region */
							BEFORE_IMAGE_NEEDED(read_before_image, cse, csa, csd, new_blk,
										before_image_needed);
							if (!before_image_needed)
								cse->old_block = NULL;
							else
							{
								block_is_free = WAS_FREE(cse->blk_prior_state);
								cse->old_block = t_qread(new_blk,
										(sm_int_ptr_t)&cse->cycle, &cse->cr);
								old_block = (blk_hdr_ptr_t)cse->old_block;
								if (NULL == old_block)
								{
									status = (enum cdb_sc)rdfail_detail;
									break;
								}
								if (!WAS_FREE(cse->blk_prior_state) && (NULL != jbp)
								    && (old_block->tn < jbp->epoch_tn))
								{	/* Compute CHECKSUM for writing PBLK record before crit.
									 * It is possible that we are reading a block that is
									 * actually marked free in the bitmap (due to concurrency
									 * issues at this point). Therefore we might be actually
									 * reading uninitialized block headers and in turn a bad
									 * value of "old_block->bsiz". Restart if we ever access a
									 * buffer whose size is greater than the db block size.
									 */
									bsiz = old_block->bsiz;
									if (bsiz > csd->blk_size)
									{
										status = cdb_sc_lostbmlcr;
										break;
									}
									JNL_GET_CHECKSUM_ACQUIRED_BLK(cse, csd, csa,
													old_block, bsiz);
								}
							}
							cse->blk = new_blk;
							csa->tp_hint = new_blk;
							cse->mode = gds_t_acquired;
							/* Assert that in final retry total_blks (private and shared) are in sync */
							assert((CDB_STAGNATE > t_tries) || !is_mm
									|| (csa->total_blks == csa->ti->total_blks));
							assert((CDB_STAGNATE > t_tries) || (cse->blk < csa->ti->total_blks));
						}	/* if (gds_t_create == cse->mode) */
					}	/* for (all cw_set_elements) */
					if (NULL == si->first_cw_bitmap)
					{
						assert(last_cw_set_before_maps);
						si->first_cw_bitmap = last_cw_set_before_maps->next_cw_set;
					}
					if ((cdb_sc_normal == status) && 0 != csd->dsid)
					{
						for (ks = si->kill_set_head; NULL != ks; ks = ks->next_kill_set)
						{
							if (ks->used)
							{
								tp_kill_bitmaps = TRUE;
								break;
							}
						}
					}
					if (NULL != si->kill_set_head)
					{
						DEBUG_ONLY(
							for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
								assert(csa->now_crit == (FILE_INFO(tr->reg)->s_addrs.now_crit));
						)
						if (!csa->now_crit)
							WAIT_ON_INHIBIT_KILLS(cnl, MAXWAIT2KILL);
						/* temp_si is to maintain index into sgm_info_ptr list till which DECR_CNTs
						 * have to be done incase abnormal status or tp_tend fails/succeeds
						 */
						temp_si = si->next_sgm_info;
					}
				} else	/* if (at least one update in segment) */
					assert(0 == si->cw_set_depth);
			}	/* for (all segments in the transaction) */
			if (cdb_sc_normal != status)
			{
				t_fail_hist[t_tries] = status;
				if (cdb_sc_blkmod == status)
				{	/* It is possible the call to "t_qread" (in "bm_getfree" or in "op_tcommit")
					 * caused the cdb_sc_blkmod status. In this case, it would have invoked the
					 * TP_TRACE_HIST_MOD macro to note down restart related details but would have
					 * used a blkmod type of "tp_blkmod_t_qread". But we need to differentiate this
					 * from a call to "t_qread" outside of the TCOMMIT which can cause the same blkmod.
					 * Hence using a separate type (tp_blkmod_op_tcommit) to indicate this is a call
					 * to "t_qread" inside "op_tcommit".
					 */
					TREF(blkmod_fail_type) = tp_blkmod_op_tcommit;
				}
				SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, cnl, status);
				TP_RETRY_ACCOUNTING(csa, cnl);
			}
			if ((cdb_sc_normal != status) || !tp_tend())
			{	/* Commit failed BEFORE invoking or DURING "tp_tend" */
				DEBUG_ONLY(prev_status = status;)
				if (cdb_sc_normal == status) /* get failure return code from tp_tend (stored in t_fail_hist) */
					status = (enum cdb_sc)t_fail_hist[t_tries];
				assert(cdb_sc_normal != status);	/* else will go into an infinite try loop */
				DEBUG_ONLY(
					for (si = first_sgm_info; si != temp_si; si = si->next_sgm_info)
						assert(NULL == si->kip_csa);
				)
				if (cdb_sc_gbloflow == status)
				{
					assert(cse);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_GBLOFLOW, 2,
						DB_LEN_STR(cse->blk_target->gd_csa->region));
					RTS_ERROR_CSA_ABT(csa, VARLSTCNT(4) ERR_GBLOFLOW, 2,
						DB_LEN_STR(cse->blk_target->gd_csa->region));
				} else if (!skip_invoke_restart)
					INVOKE_RESTART;
				GTMTRIG_ONLY(DBGTRIGR((stderr, "op_tcommit: Return status = %d\n", status));)
				return status;	/* return status to caller who cares about it */
			} else
			{	/* Now that tp_tend() is done and we do not hold crit, check if we had an unhandled IO timer pop. */
				if (unhandled_stale_timer_pop)
					process_deferred_stale();
				for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
				{
					csa = si->tp_csa;
					cnl = csa->nl;
					INCR_GVSTATS_COUNTER(csa, cnl, n_tp_blkread, si->num_of_blks);
					if (!si->update_trans)
					{
						INCR_GVSTATS_COUNTER(csa, cnl, n_tp_readonly, 1);
						continue;
					}
					INCR_GVSTATS_COUNTER(csa, cnl, n_tp_readwrite, 1);
					INCR_GVSTATS_COUNTER(csa, cnl, n_tp_blkwrite, si->cw_set_depth);
					GVSTATS_SET_CSA_STATISTIC(csa, db_curr_tn, si->start_tn);
					TP_TEND_CHANGE_REG(si);
					wcs_timer_start(gv_cur_region, TRUE);
					if (si->backup_block_saved)
						backup_buffer_flush(gv_cur_region);
				}
			}
			assert(UNIX_ONLY(jgbl.onlnrlbk || TREF(in_trigger_upgrade) || ) (0 == have_crit(CRIT_HAVE_ANY_REG)));
			csa = jnl_fence_ctl.fence_list;
			if ((JNL_FENCE_LIST_END != csa) && jgbl.wait_for_jnl_hard && !is_updproc && !mupip_jnl_recover)
			{	/* For mupip journal recover all transactions applied during forward phase are treated as
				 * BATCH transaction for performance gain, since the recover command can be reissued like
				 * a batch restart. Similarly update process considers all transactions as BATCH */
				do
				{	/* only those regions that are actively journaling and had a TCOM record
					 * written as part of this TP transaction will appear in the list.
					 */
					assert(NULL != csa);
					TP_CHANGE_REG_IF_NEEDED(csa->jnl->region);
					jnl_wait(csa->jnl->region);
					csa = csa->next_fenced;
				} while (JNL_FENCE_LIST_END != csa);
			}
		} else if ((CDB_STAGNATE <= t_tries) && (NULL != tp_reg_list))
		{	/* this is believed to be a case of M-lock work with no database work. release crit on all regions */
			UNIX_ONLY(assert(!jgbl.onlnrlbk)); /* online rollback cannot reach here */
			for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			{
				assert(FILE_INFO(tr->reg)->s_addrs.now_crit);
				rel_crit(tr->reg);
			}
			temp_si = NULL;
		} else
			temp_si = NULL;
		assert(UNIX_ONLY(jgbl.onlnrlbk || TREF(in_trigger_upgrade) || ) (0 == have_crit(CRIT_HAVE_ANY_REG)));

		/* Commit was successful. Reset TP related global variables (just like is done in "op_trollback.c"). */
		dollar_trestart = 0;
		t_tries = 0;

		/* the following section is essentially deferred garbage collection, freeing release block a bitmap at a time */
		if (NULL != first_sgm_info)
		{
			for (si = first_sgm_info; si != temp_si; si = si->next_sgm_info)
			{
				if (NULL == si->kill_set_head)
				{
					assert(NULL == si->kip_csa);
					continue;	/* no kills in this segment */
				}
				ENABLE_WBTEST_ABANDONEDKILL;
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
				sgm_info_ptr = si;	/* needed in gvcst_expand_free_subtree */
				gvcst_expand_free_subtree(si->kill_set_head);
				assert(NULL != si->kill_set_head);
				DECR_KIP(cs_data, cs_addrs, si->kip_csa);
			}		/* for (all segments in the transaction) */
			assert(NULL == temp_si || NULL == si->kill_set_head);
		}	/* if (kills in the transaction) */
		tp_clean_up(TP_COMMIT);
		gv_cur_region = save_cur_region;
		TP_CHANGE_REG(gv_cur_region);
		jnlpool = save_jnlpool;
#		ifdef DEBUG
		/* See comment in similar code before tp_tend call above */
		if (!forw_recov_lgtrig_only && (!jgbl.forw_phase_recovery || (NULL != gv_target)))
			DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
#		endif
		/* Cancel or clear any pending TP timeout only if real commit (i.e. outermost commit) */
		if (in_timed_tn)
			(*tp_timeout_clear_ptr)(TRUE);
	} else		/* an intermediate commit */
		tp_incr_commit();
	assert(dollar_tlevel);
	tp_unwind(dollar_tlevel - 1, COMMIT_INVOCATION, NULL);
	if (!dollar_tlevel) /* real commit */
	{
		/* Transaction is complete as the outer transaction has been committed. Check now to see if any statsDB
		 * region initializations were deferred and drive them now if they were.
		 */
		if (NULL != TREF(statsDB_init_defer_anchor))
			gvcst_deferred_init_statsDB();
		JOBINTR_TP_RETHROW; /* rethrow job interrupt($ZINT) if $ZTEXIT, when coerced to boolean, is true */
	}
	GTMTRIG_ONLY(DBGTRIGR((stderr, "op_tcommit: Return NORMAL status\n"));)
	return cdb_sc_normal;
}
