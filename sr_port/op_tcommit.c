/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
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

error_def(ERR_GBLOFLOW);
error_def(ERR_GVIS);
error_def(ERR_TLVLZERO);
error_def(ERR_TPRETRY);

GBLREF	short			dollar_tlevel, dollar_trestart;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	tp_frame		*tp_pointer;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info, *sgm_info_ptr;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	unsigned char		rdfail_detail;
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_addr			*gd_header;
GBLREF	bool			tp_kill_bitmaps;
GBLREF	gv_namehead		*gv_target;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t		is_updproc;
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */

void	op_tcommit(void)
{
	boolean_t		blk_used, is_mm;
	sm_uc_ptr_t		bmp;
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end;
	unsigned int		ctn;
	int			cw_depth, cycle, len, old_cw_depth, sleep_counter;
	sgmnt_addrs		*csa, *next_csa;
	sgmnt_data_ptr_t	csd;
	sgm_info		*si, *temp_si;
	enum cdb_sc		status;
	cw_set_element		*cse, *last_cw_set_before_maps, *csetemp, *first_cse;
	blk_ident		*blk, *blk_top, *next_blk;
	block_id		bit_map, next_bm, new_blk, temp_blk;
	cache_rec_ptr_t		cr;
	kill_set		*ks;
	off_chain		chain1;
	tp_region		*tr;
	/* for MM extend */
	cw_set_element		*update_cse;
	blk_segment		*seg, *stop_ptr, *array;
	sm_long_t		delta;
	sm_uc_ptr_t		old_db_addrs[2];
	srch_blk_status		*t1;
	jnl_buffer_ptr_t	jbp; /* jbp is non-NULL only if before-image journaling */
	blk_hdr_ptr_t		old_block;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress
						    * This is used to read before-images of blocks whose cs->mode is gds_t_create */
	unsigned int		bsiz;
	gd_region		*save_cur_region;	/* saved copy of gv_cur_region before TP_CHANGE_REG modifies it */

	if (0 == dollar_tlevel)
		rts_error(VARLSTCNT(1) ERR_TLVLZERO);
	assert(0 == jnl_fence_ctl.level);
	status = cdb_sc_normal;
	tp_kill_bitmaps = FALSE;

	if (1 == dollar_tlevel)					/* real commit */
	{
		save_cur_region = gv_cur_region;
		DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
		if (NULL != first_sgm_info)	/* if (database work in the transaction) */
		{
			for (temp_si = si = first_sgm_info; (cdb_sc_normal == status) && (NULL != si); si = si->next_sgm_info)
			{
				sgm_info_ptr = si;		/* for t_qread (at least) */
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
				csa = cs_addrs;
				csd = cs_data;
				is_mm = (dba_mm == cs_addrs->hdr->acc_meth);
				si->cr_array_index = 0;
				if (!is_mm && (si->cr_array_size < (si->num_of_blks + (si->cw_set_depth * 2))))
				{	/* reallocate a bigger cr_array. We need atmost read-set (si->num_of_blks) +
					 * write-set (si->cw_set_depth) + bitmap-write-set (a max. of si->cw_set_depth)
					 */
					free(si->cr_array);
					si->cr_array_size = si->num_of_blks + (si->cw_set_depth * 2);
					si->cr_array = (cache_rec_ptr_ptr_t)malloc(sizeof(cache_rec_ptr_t) * si->cr_array_size);
				}
				assert(!is_mm || (0 == si->cr_array_size && NULL == si->cr_array));
				/* whenever si->first_cw_set is non-NULL, ensure that si->update_trans is TRUE */
				assert((NULL == si->first_cw_set) || si->update_trans);
				/* whenever si->first_cw_set is NULL, ensure that si->update_trans is FALSE
				 * except when the set noop optimization is enabled */
				assert((NULL != si->first_cw_set) || !si->update_trans || gvdupsetnoop);
				if (NULL != si->first_cw_set)
				{
					assert(0 != si->cw_set_depth);
					cw_depth = si->cw_set_depth;
					/* Caution : since csa->backup_in_prog is initialized below only if si->first_cw_set is
					 * non-NULL, it should be used in "tp_tend" only within an if (NULL != si->first_cw_set)
					 */
					csa->backup_in_prog = (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb);
					jbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
					read_before_image = ((NULL != jbp) || csa->backup_in_prog);
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
							old_db_addrs[0] = csa->db_addrs[0];
							old_db_addrs[1] = csa->db_addrs[1];
							first_cse = si->first_cw_set;
							TRAVERSE_TO_LATEST_CSE(first_cse);
							while (FILE_EXTENDED == (new_blk = bm_getfree(cse->blk, &blk_used,
								cw_depth, first_cse, &si->cw_set_depth)))
							{
								assert(is_mm);
								wcs_mm_recover(si->gv_cur_region);
								delta = (sm_long_t)((sm_uc_ptr_t)csa->hdr - (sm_uc_ptr_t)csd);
								csd = csa->hdr;
								/* update cse's update array and old_block */
								for (update_cse = si->first_cw_set;  NULL != update_cse;
									update_cse = update_cse->next_cw_set)
								{
									TRAVERSE_TO_LATEST_CSE(update_cse);
									if (gds_t_writemap != update_cse->mode)
									{
										array = (blk_segment *)update_cse->upd_addr;
										stop_ptr = update_cse->first_copy ?
												array : array + 1;
										seg = (blk_segment *)array->addr;
										while (seg != stop_ptr)
										{
											if ((old_db_addrs[0] < seg->addr)
											&& (old_db_addrs[1] >= seg->addr))
												seg->addr += delta;
											seg--;
										}
									}
									if (NULL != update_cse->old_block)
									{
										assert((old_db_addrs[0] < update_cse->old_block) &&
											(old_db_addrs[1] > update_cse->old_block));
										update_cse->old_block += delta;
									}
								}
								/* update all the tp_hist */
								for (t1 = si->first_tp_hist;  t1 != si->last_tp_hist; t1++)
								{
									if ((old_db_addrs[0] < t1->buffaddr)
										&& (old_db_addrs[1] >= t1->buffaddr))
										t1->buffaddr += delta;
								}
								/* In case the while loop above needs to repeat more than once,
								 * the mmaped addresses for the file's start and end could have
								 * changed if wcs_mm_recover() caused a file extension.  In that
								 * case, reset the limits to the new values.
								 */
								if (csa->db_addrs[0] != old_db_addrs[0])
								{
									old_db_addrs[0] = csa->db_addrs[0];
									old_db_addrs[1] = csa->db_addrs[1];
									csd = csa->hdr;
								}
							}
							if (0 > new_blk)
							{
								GET_CDB_SC_CODE(new_blk, status); /* code is set in status */
								break;	/* transaction must attempt restart */
							}
							/* No need to write before-image in case the block is FREE. In case the
							 * database had never been fully upgraded from V4 to V5 format (after the
							 * MUPIP UPGRADE), all RECYCLED blocks can basically be considered FREE
							 * (i.e. no need to write before-images since backward journal recovery
							 * will never be expected to take the database to a point BEFORE the
							 * mupip upgrade).
							 */
							if (read_before_image && blk_used && csd->db_got_to_v5_once)
							{
								cse->old_block = t_qread(new_blk,
										(sm_int_ptr_t)&cse->cycle, &cse->cr);
								old_block = (blk_hdr_ptr_t)cse->old_block;
								if (NULL == old_block)
								{
									status = (enum cdb_sc)rdfail_detail;
									break;
								}
								if ((NULL != jbp) && (old_block->tn < jbp->epoch_tn))
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
									JNL_GET_CHECKSUM_ACQUIRED_BLK(cse, csd, old_block, bsiz);
								}
							} else
								cse->old_block = NULL;
							cse->blk = new_blk;
							cse->mode = gds_t_acquired;
							assert(GDSVCURR == cse->ondsk_blkver);
							assert(CDB_STAGNATE > t_tries ||
								(is_mm ? (cse->blk < csa->total_blks)
										: (cse->blk < csa->ti->total_blks)));
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
							WAIT_ON_INHIBIT_KILLS(csa->nl, MAXWAIT2KILL);
						/* temp_si is to maintain index into sgm_info_ptr list till which DECR_CNTs
						 * have to be done incase abnormal status or tp_tend fails/succeeds
						 */
						temp_si = si->next_sgm_info;
					}
				} else	/* if (at least one set in segment) */
					assert(0 == si->cw_set_depth);
			}	/* for (all segments in the transaction) */
			if (cdb_sc_normal != status)
			{
				t_fail_hist[t_tries] = status;
				SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
				TP_RETRY_ACCOUNTING(csa, csa->nl, status);
			}
			if ((cdb_sc_normal == status) && tp_tend())
				;
			else	/* commit failed BEFORE invoking or DURING "tp_tend" */
			{
				assert(cdb_sc_normal != t_fail_hist[t_tries]);	/* else will go into an infinite try loop */
				DEBUG_ONLY(
					for (si = first_sgm_info;  si != temp_si; si = si->next_sgm_info)
						assert(!si->kip_incremented);
				)
				if (cdb_sc_gbloflow == status)
				{
					if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, cse->blk_target->last_rec, TRUE)))
						end = &buff[MAX_ZWR_KEY_SZ - 1];
					rts_error(VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
				} else
					INVOKE_RESTART;
				return;
			}
			assert(0 == have_crit(CRIT_HAVE_ANY_REG));
			if (jgbl.wait_for_jnl_hard && !is_updproc && !mupip_jnl_recover)
			{	/* For mupip journal recover all transactions applied during forward phase are treated as
			   	 * BATCH transaction for performance gain, since the recover command can be reissued like
			   	 * a batch restart. Similarly update process considers all transactions as BATCH */
				if (JNL_FENCE_LIST_END != (csa = jnl_fence_ctl.fence_list))
				{
					for ( ; JNL_FENCE_LIST_END != csa;  csa = csa->next_fenced)
					{	/* only those regions that are actively journaling will appear in the list: */
						TP_CHANGE_REG_IF_NEEDED(csa->jnl->region);
						jnl_wait(csa->jnl->region);
					}
				}
			}
		} else if ((CDB_STAGNATE <= t_tries) && (NULL != tp_reg_list))
		{	/* this is believed to be a case of M-lock work with no database work. release crit on all regions */
			for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			{
				assert(FILE_INFO(tr->reg)->s_addrs.now_crit);
				rel_crit(tr->reg);
			}
		}
		assert(0 == have_crit(CRIT_HAVE_ANY_REG));
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
					assert(!si->kip_incremented);
					continue;	/* no kills in this segment */
				}
				GTM_WHITE_BOX_TEST(WBTEST_ABANDONEDKILL, sleep_counter, SLEEP_ONE_MIN);
#				ifdef DEBUG
				if (SLEEP_ONE_MIN == sleep_counter)
				{
					assert(gtm_white_box_test_case_enabled);
					while (1 <= sleep_counter)
						wcs_sleep(sleep_counter--);
				}
#				endif
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
				sgm_info_ptr = si;	/* needed in gvcst_expand_free_subtree */
				gvcst_expand_free_subtree(si->kill_set_head);
				assert(NULL != si->kill_set_head);
				DECR_KIP(cs_data, cs_addrs, si->kip_incremented);
			}		/* for (all segments in the transaction) */
			assert(NULL == temp_si || NULL == si->kill_set_head);
		}	/* if (kills in the transaction) */
		tp_clean_up(FALSE);	/* Not the rollback type of cleanup */
		gv_cur_region = save_cur_region;
		TP_CHANGE_REG(gv_cur_region);
		DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
		/* Cancel or clear any pending TP timeout only if real commit (i.e. outermost commit) */
		(*tp_timeout_clear_ptr)();
	} else		/* an intermediate commit */
		tp_incr_commit();
	assert(0 < dollar_tlevel);
	tp_unwind(dollar_tlevel - 1, COMMIT_INVOCATION);
	if (0 == dollar_tlevel) /* real commit */
		JOBINTR_TP_RETHROW; /* rethrow job interrupt($ZINT) if $ZTEXIT, when coerced to boolean, is true */
}
