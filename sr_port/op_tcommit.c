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

#include "hashtab_int4.h"	/* needed for tp.h */
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

#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif

#ifdef GTM_TRIGGER
#include <rtnhdr.h>		/* for rtn_tabent in gv_trigger.h */
#include "gv_trigger.h"
#include "gtm_trigger.h"
#endif

GBLREF	uint4			dollar_tlevel;
GBLREF	uint4 			dollar_trestart;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	tp_frame		*tp_pointer;
GBLREF	gd_region		*gv_cur_region;
GBLREF  gv_key			*gv_currkey;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info, *sgm_info_ptr;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	unsigned char		rdfail_detail;
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_addr			*gd_header;
GBLREF	boolean_t		tp_kill_bitmaps;
GBLREF	gv_namehead		*gv_target;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t		is_updproc;
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	boolean_t		block_is_free;
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_INVOKE_RESTART;
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
#endif

error_def(ERR_GBLOFLOW);
error_def(ERR_GVIS);
error_def(ERR_TLVLZERO);
error_def(ERR_TPRETRY);
#ifdef GTM_TRIGGER
error_def(ERR_TRIGTCOMMIT);
error_def(ERR_TCOMMITDISALLOW);
#endif

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
			array = (blk_segment *)cse->upd_addr;
			stop_ptr = cse->first_copy ? array : array + 1;
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
	boolean_t		blk_used, is_mm, was_crit;
	sm_uc_ptr_t		bmp;
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end;
	unsigned int		ctn;
	int			cw_depth, cycle, len, old_cw_depth;
	sgmnt_addrs		*csa, *next_csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sgm_info		*si, *temp_si;
	enum cdb_sc		status;
	cw_set_element		*cse, *last_cw_set_before_maps, *csetemp, *first_cse;
	blk_ident		*blk, *blk_top, *next_blk;
	block_id		bit_map, next_bm, new_blk, temp_blk;
	cache_rec_ptr_t		cr;
	kill_set		*ks;
	off_chain		chain1;
	tp_region		*tr;
	sm_uc_ptr_t		old_db_addrs[2]; /* for MM extend */
	jnl_buffer_ptr_t	jbp; /* jbp is non-NULL only if before-image journaling */
	blk_hdr_ptr_t		old_block;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress
						    * This is used to read before-images of blocks whose cs->mode is gds_t_create */
	unsigned int		bsiz;
	gd_region		*save_cur_region;	/* saved copy of gv_cur_region before TP_CHANGE_REG modifies it */
	boolean_t		before_image_needed;
	boolean_t		skip_invoke_restart;
	uint4			update_trans;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef GTM_TRIGGER
	DBGTRIGR((stderr, "op_tcommit: Entry from 0x"lvaddr"\n", caller_id()));
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
	GTMTRIG_ONLY(
		/* The value of $ztlevel at the time of the TSTART, i.e. tstart_trigger_depth, can never be GREATER than
		 * the current $ztlevel as otherwise a TPQUIT error would have been issued as part of the QUIT of the
		 * M frame of the TSTART (which has a deeper trigger depth). Assert that.
		 */
		assert(tstart_trigger_depth <= gtm_trigger_depth);
	)
	if (1 == dollar_tlevel)		/* real commit */
	{
		GTMTRIG_ONLY(
			if (gtm_trigger_depth != tstart_trigger_depth)
			{	/* TCOMMIT to $tlevel=0 is being attempted at a trigger depth which is NOT EQUAL TO the trigger
				 * depth at the time of the TSTART. This means we have a gvcst_put/gvcst_kill frame in the
				 * C stack that invoked us through gtm_trigger and is in an incomplete state waiting for the
				 * trigger invocation to be done before completing the explicit (outside-trigger) update.
				 * Cannot commit such a transaction. Issue error.
				 */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGTCOMMIT, 2, gtm_trigger_depth,
						tstart_trigger_depth);
			}
			if (tp_pointer->cannot_commit)
			{	/* If this TP transaction was implicit, any unhandled error when crossing the trigger boundary
				 * would have caused a rethrow of the error in the M frame that held the non-TP update which
				 * would then have invoked "error_return" that would in turn have rolled back the implicit TP
				 * transaction so we should never see an implicit TP inside op_tcommit.
				 */
				assert(!tp_pointer->implicit_tstart);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TCOMMITDISALLOW);
			}
		)
		save_cur_region = gv_cur_region;
		DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
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
#					ifdef GTM_SNAPSHOT
					if (SNAPSHOTS_IN_PROG(cnl))
					{
						/* If snapshot context is not already created, then create one now to be used
						 * by this transaction. If context creation failed (for instance, on snapshot
						 * file open fail), then SS_INIT_IF_NEEDED sets csa->snapshot_in_prog is to
						 * FALSE.
						 */
						SS_INIT_IF_NEEDED(csa, cnl);
					} else
						CLEAR_SNAPSHOTS_IN_PROG(csa);
#					endif
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
					}
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
							cse->mode = gds_t_acquired;
							assert(GDSVCURR == cse->ondsk_blkver);
							/* Assert that in final retry total_blks (private and shared) are in sync */
							assert((CDB_STAGNATE > t_tries) || !is_mm
									|| (csa->total_blks == csa->ti->total_blks));
							assert((CDB_STAGNATE > t_tries) || (cse->blk < csa->ti->total_blks));
						}	/* if (gds_t_create == cse->mode) */
					}	/* for (all cw_set_elements) */
					if (NULL == si->first_cw_bitmap)
						si->first_cw_bitmap = last_cw_set_before_maps->next_cw_set;
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
				SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, cnl, status);
				TP_RETRY_ACCOUNTING(csa, cnl);
			}
			if ((cdb_sc_normal == status) && tp_tend())
				;
			else	/* commit failed BEFORE invoking or DURING "tp_tend" */
			{
				if (cdb_sc_normal == status) /* get failure return code from tp_tend (stored in t_fail_hist) */
					status = (enum cdb_sc)t_fail_hist[t_tries];
				assert(cdb_sc_normal != status);	/* else will go into an infinite try loop */
				DEBUG_ONLY(
					for (si = first_sgm_info;  si != temp_si; si = si->next_sgm_info)
						assert(NULL == si->kip_csa);
				)
				if (cdb_sc_gbloflow == status)
				{
					if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
						end = &buff[MAX_ZWR_KEY_SZ - 1];
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
				} else if (!skip_invoke_restart)
					INVOKE_RESTART;
				GTMTRIG_ONLY(DBGTRIGR((stderr, "op_tcommit: Return status = %d\n", status));)
				return status;	/* return status to caller who cares about it */
			}
			assert(UNIX_ONLY(jgbl.onlnrlbk ||) (0 == have_crit(CRIT_HAVE_ANY_REG)));
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
		}
		assert(UNIX_ONLY(jgbl.onlnrlbk ||) (0 == have_crit(CRIT_HAVE_ANY_REG)));
		/* Commit was successful */
		dollar_trestart = 0;
		t_tries = 0;
		/* the following section is essentially deferred garbage collection, freeing release block a bitmap at a time */
		if (NULL != first_sgm_info)
		{
			for (si = first_sgm_info;  si != temp_si;  si = si->next_sgm_info)
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
		tp_clean_up(FALSE);	/* Not the rollback type of cleanup */
		gv_cur_region = save_cur_region;
		TP_CHANGE_REG(gv_cur_region);
		DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
		/* Cancel or clear any pending TP timeout only if real commit (i.e. outermost commit) */
		(*tp_timeout_clear_ptr)();
	} else		/* an intermediate commit */
		tp_incr_commit();
	assert(dollar_tlevel);
	tp_unwind(dollar_tlevel - 1, COMMIT_INVOCATION, NULL);
	if (!dollar_tlevel) /* real commit */
		JOBINTR_TP_RETHROW; /* rethrow job interrupt($ZINT) if $ZTEXIT, when coerced to boolean, is true */
	GTMTRIG_ONLY(DBGTRIGR((stderr, "op_tcommit: Return NORMAL status\n"));)
	return cdb_sc_normal;
}
