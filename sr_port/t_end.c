/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
#include "gtm_stdio.h"
#endif
#include "gtm_time.h"

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "ccp.h"
#include "error.h"
#include "interlock.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "gdsbgtr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "mupipbckup.h"
#include "cache.h"
#include "gt_timer.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_retry.h"
#include "t_commit_cleanup.h"
#include "send_msg.h"
#include "bm_getfree.h"
#include "rc_cpt_ops.h"
#include "rel_quant.h"
#include "wcs_flu.h"
#include "jnl_write_aimg_rec.h"
#include "jnl_write_pblk.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#ifdef UNIX
#include "process_deferred_stale.h"
#endif
#include "t_end.h"
#include "add_inter.h"

#define BLOCK_FLUSHING(x) (csa->hdr->clustered && x->flushing && !CCP_SEGMENT_STATE(cs_addrs->nl,CCST_MASK_HAVE_DIRTY_BUFFERS))

GBLREF bool			certify_all_blocks, rc_locked;
GBLREF unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF cache_rec_ptr_t		cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF unsigned int		cr_array_index;
GBLREF boolean_t		block_saved;
GBLREF bool			update_trans;
GBLREF cw_set_element		cw_set[];		/* create write set. */
GBLREF gd_region		*gv_cur_region;
GBLREF gv_namehead		*gv_target;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF short			dollar_tlevel;
GBLREF trans_num		start_tn;
GBLREF unsigned int		t_tries;
GBLREF uint4			t_err, process_id;
GBLREF unsigned char		cw_set_depth, cw_map_depth;
GBLREF unsigned char		rdfail_detail;
GBLREF jnlpool_addrs		jnlpool;
GBLREF jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF uint4			cumul_jnl_rec_len;
GBLREF bool			is_standalone;
GBLREF bool			is_db_updater, run_time;
GBLREF boolean_t		is_updproc;
GBLREF seq_num			seq_num_one;
GBLREF boolean_t		mu_reorg_process;
GBLREF boolean_t		dse_running;
GBLREF boolean_t		unhandled_stale_timer_pop;
GBLREF jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF inctn_opcode_t		inctn_opcode;
GBLREF uint4			gbl_jrec_time;	/* see comment in gbldefs.c for usage */
GBLREF seq_num			max_resync_seqno;
GBLREF boolean_t 		kip_incremented;
GBLREF boolean_t		need_kip_incr;
DEBUG_ONLY(GBLREF uint4		cumul_index;
	   GBLREF uint4		cu_jnl_index;
	  )

LITREF int			jnl_fixed_size[];

/* This macro isn't enclosed in parantheses to allow for optimizations */
#define VALIDATE_CYCLE(history)						\
if (history)								\
{									\
	for (t1 = history->h;  t1->blk_num;  t1++)			\
	{								\
		if (t1->cr->cycle != t1->cycle)				\
		{		/* cache slot has been stolen */	\
			assert(FALSE == csa->now_crit);			\
			status = cdb_sc_cyclefail;			\
			goto failed;					\
		}							\
	}								\
}

int	t_end(srch_hist *hist1, srch_hist *hist2)
{
	srch_hist		*hist;
	blk_hdr_ptr_t		bp;
	bt_rec_ptr_t		bt;
	bool			blk_used, is_mm, was_crit;
	cache_rec_ptr_t		cr, cr0, cr1;
	cw_set_element		*cs, *cs_top, *nxt;
	enum cdb_sc		status;
	file_control		*fc;
	int			int_depth;
	uint4			jnl_status;
	jnl_buffer_ptr_t	jbp;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sgm_info		*dummysi = NULL;	/* needed as a dummy parameter for {mm,bg}_update */
	srch_blk_status		*t1;
	trans_num		start_ctn, save_early_tn, valid_thru, ctn, tnque_earliest_tn;
	unsigned char		cw_depth;
	jnldata_hdr_ptr_t	jnl_header;
	uint4			total_jnl_rec_size;

	error_def(ERR_GVKILLFAIL);
	error_def(ERR_NOTREPLICATED);

	assert(hist1 != hist2);
	csa = cs_addrs;
	csd = csa->hdr;
	is_mm = (dba_mm == csd->acc_meth);
	status = cdb_sc_normal;
	assert(cs_data == csd);
	assert((t_tries < CDB_STAGNATE) || csa->now_crit);
	assert(0 == dollar_tlevel);
	if (csd->wc_blocked || (is_mm && (csa->total_blks != csa->ti->total_blks)))
	{ /* If blocked, or we have MM and file has been extended, force repair */
		status = cdb_sc_helpedout;	/* force retry with special status so philanthropy isn't punished */
		goto failed;
	} else
	{
		if (0 == cw_set_depth && start_tn == csa->ti->early_tn)
		{	/* read with no change to the transaction history */
			if (!is_mm)
			{
				VALIDATE_CYCLE(hist1);
				VALIDATE_CYCLE(hist2);
			}
			if (csa->now_crit)
			{
				rel_crit(gv_cur_region);
			}
			UNIX_ONLY(if (unhandled_stale_timer_pop) process_deferred_stale();)
			return csa->ti->curr_tn;
		}
	}
	cr_array_index = 0;
	ESTABLISH_RET(t_ch, 0);
	if ((0 != cw_set_depth) && (gds_t_writemap == cw_set[0].mode))
		cw_depth = 0;				/* freeing a block from gvcst_kill or, reorg */
	else
		cw_depth = cw_set_depth;
	csa->jnl_before_image = csd->jnl_before_image;
	if (0 != cw_depth)
	{
		for (cs = cw_set, cs_top = cw_set + cw_depth;
		     (cs < cs_top) && (gds_t_write_root != cs->mode);
		     cs++)
		{
			if (gds_t_create == cs->mode)
			{
				int_depth = (int)cw_set_depth;
				if (0 > (cs->blk = bm_getfree(cs->blk, &blk_used, cw_depth, cw_set, &int_depth)))
				{
					if (FILE_EXTENDED == cs->blk)
					{
						status = cdb_sc_helpedout;
						assert(is_mm);
					} else
					{
						GET_CDB_SC_CODE(cs->blk, status);	/* code is set in status */
					}
					break;
				}
				if (!blk_used  ||  !(JNL_ENABLED(csd)  &&  csa->jnl_before_image))
					cs->old_block = 0;
				else
				{
					cs->old_block = t_qread(cs->blk, (sm_int_ptr_t)&cs->cycle, &cs->cr);
					if (NULL == cs->old_block)
					{
						status = rdfail_detail;
						break;
					}
				}
				assert(cs->blk < csa->ti->total_blks);
				cs->mode = gds_t_acquired;
			}
		}
	}
	block_saved = FALSE;
	if (cdb_sc_normal == status)
	{
		if (!csa->now_crit)
		{
			if (update_trans)
			{	/* Get more space if needed. This is done outside crit so that
				 * any necessary IO has a chance of occurring outside crit.
				 * The available space must be double-checked inside crit. */
				if (!is_mm && (csa->nl->wc_in_free < (int4)(cw_set_depth + 1))
					   && !wcs_get_space(gv_cur_region, cw_set_depth + 1, 0))
				{
					SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
					status = cdb_sc_cacheprob;
					REVERT;
					goto failed;
				}
				for (;;)
				{
					grab_crit(gv_cur_region);
					if (FALSE == csd->freeze)
						break;
					rel_crit(gv_cur_region);
					while (csd->freeze)
						hiber_start(1000);
				}
			} else
				grab_crit(gv_cur_region);
		}
		assert (csd == csa->hdr);
		valid_thru = ctn = csa->ti->curr_tn;
		if (!is_mm)
			tnque_earliest_tn = ((th_rec_ptr_t)((sm_uc_ptr_t)csa->th_base + csa->th_base->tnque.fl))->tn;
		if (0 != cw_set_depth)
			valid_thru++;
		for (hist = hist1;  NULL != hist && cdb_sc_normal == status;  hist = (hist == hist1) ? hist2 : NULL)
		{
			for (t1 = hist->h;  t1->blk_num;  t1++)
			{
				if (is_mm)
				{
					if (t1->tn <= ((blk_hdr_ptr_t)(t1->buffaddr))->tn)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_blkmod;
						break;
					}
				} else
				{
					bt = bt_get(t1->blk_num);
					if (NULL == bt)
					{
						if (t1->tn <= tnque_earliest_tn)
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_losthist;
							break;
						}
						cr = db_csh_get(t1->blk_num);
					} else
					{
						if (BLOCK_FLUSHING(bt))
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_blockflush;
							break;
						}
						if (t1->tn <= bt->tn)
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_blkmod;
							break;
						}
						if (CR_NOTVALID == bt->cache_index)
							cr = db_csh_get(t1->blk_num);
						else
						{
							cr = (cache_rec_ptr_t)GDS_REL2ABS(bt->cache_index);
							if (cr->blk != bt->blk)
							{
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								GTMASSERT;
							}
						}
					}
					if ((cache_rec_ptr_t)CR_NOTVALID == cr)
					{
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
						status = cdb_sc_cacheprob;
						break;
					}
					if ((NULL == cr) || (cr->cycle != t1->cycle) ||
					    ((sm_long_t)GDS_REL2ABS(cr->buffaddr) != (sm_long_t)t1->buffaddr))
					{
						if (cr && bt &&(cr->blk != bt->blk))
						{
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							GTMASSERT;
						}
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostcr;
						break;
					}
					cr_array[cr_array_index++] = cr;
					/* only way to have in_cw_set to be TRUE is in the case
					 * where both the histories passed contain this particular block.
					 */
					assert(FALSE == cr->in_cw_set || hist == hist2 && cr->blk == hist1->h[t1->level].blk_num);
					cr->in_cw_set = TRUE;
					cr->refer = TRUE;
				}
				t1->tn = valid_thru;
			}
		}
		if (cdb_sc_normal == status)
		{				/* check bit maps for usage */
			if (0 != cw_map_depth)	/* Bit maps on end from mupip_reorg */
				cw_set_depth = cw_map_depth;
			for (cs = &cw_set[cw_depth], cs_top = &cw_set[cw_set_depth]; cs < cs_top; cs++)
			{
				if (is_mm)
				{
					if (cs->tn <= ((blk_hdr_ptr_t)(cs->old_block))->tn)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_bmlmod;
						break;
					}
				} else
				{
					bt = bt_get(cs->blk);
					if (NULL == bt)
					{
						if (cs->tn <= tnque_earliest_tn)
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_lostbmlhist;
							break;
						}
						cr = db_csh_get(cs->blk);
					} else
					{
						if (BLOCK_FLUSHING(bt))
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_blockflush;
							break;
						}
						if (cs->tn <= bt->tn)
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_bmlmod;
							break;
						}
						if (CR_NOTVALID == bt->cache_index)
							cr = db_csh_get(cs->blk);
						else
						{
							cr = (cache_rec_ptr_t)GDS_REL2ABS(bt->cache_index);
							if (cr->blk != bt->blk)
							{
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								GTMASSERT;
							}
						}
					}
					if ((cache_rec_ptr_t)CR_NOTVALID == cr)
					{
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_bitmap_nullbt);
						status = cdb_sc_cacheprob;
						break;
					}
					if ((NULL == cr)  || (cr->cycle != cs->cycle) ||
					    ((sm_long_t)GDS_REL2ABS(cr->buffaddr) != (sm_long_t)cs->old_block))
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbmlcr;
						break;
					}
					cr_array[cr_array_index++] = cr;
					assert(FALSE == cr->in_cw_set);
					cr->in_cw_set = TRUE;
					cr->refer = TRUE;
				}
			}
		}
		assert(csd == csa->hdr);
		if ((cdb_sc_normal == status) && (0 != cw_set_depth))
		{
			if (!is_mm && JNL_ENABLED(csd) && csa->jnl_before_image)
			{
				for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
				{	/* Read old block for creates before got crit,
					   make sure cache record still has correct block */
					if ((gds_t_acquired == cs->mode) && (NULL != cs->old_block))
					{
						cr = db_csh_get(cs->blk);
						if ((NULL == cr) || ((cache_rec_ptr_t)CR_NOTVALID == cr)
						    || (cr->cycle != cs->cycle))
						{
							if ((cache_rec_ptr_t)CR_NOTVALID == cr)
							{
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_cwset);
								status = cdb_sc_cacheprob;
							} else
							{
								assert(CDB_STAGNATE > t_tries);
								status = cdb_sc_lostbefor;
							}
							break;
						}
						cr_array[cr_array_index++] = cr;
						assert(FALSE == cr->in_cw_set);
						cr->in_cw_set = TRUE;
						cr->refer = TRUE;
					}
				}
			}
			if (cdb_sc_normal == status)
			{
				if (REPL_ENABLED(csd) && cw_depth && (run_time || is_db_updater)
								&& inctn_invalid_op == inctn_opcode)
				{
					grab_lock(jnlpool.jnlpool_dummy_reg);
					QWASSIGN(temp_jnlpool_ctl->write_addr, jnlpool_ctl->write_addr);
					QWASSIGN(temp_jnlpool_ctl->write, jnlpool_ctl->write);
					QWASSIGN(temp_jnlpool_ctl->jnl_seqno, jnlpool_ctl->jnl_seqno);
					INT8_ONLY(assert(temp_jnlpool_ctl->write ==
								temp_jnlpool_ctl->write_addr % temp_jnlpool_ctl->jnlpool_size);)
					cumul_jnl_rec_len += sizeof(jnldata_hdr_struct);
					temp_jnlpool_ctl->write += sizeof(jnldata_hdr_struct);
					if (temp_jnlpool_ctl->write >= temp_jnlpool_ctl->jnlpool_size)
					{
						assert(temp_jnlpool_ctl->write == temp_jnlpool_ctl->jnlpool_size);
						temp_jnlpool_ctl->write = 0;
					}
					assert(QWEQ(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr));
					QWADDDW(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr, cumul_jnl_rec_len);
				}
				assert(cw_set_depth < CDB_CW_SET_SIZE);
				assert(csa->ti->early_tn == csa->ti->curr_tn);
				start_ctn = csa->ti->curr_tn;
				csa->ti->early_tn = start_ctn + 1;
				if (JNL_ENABLED(csd))
				{
					/* The JNL_SHORT_TIME done below should be done before any journal writing activity on the
					 * journal file. This is because at this stage, we have early_tn != curr_tn and hence
					 * jnl_write_logical will assume that gbl_jrec_time is appropriately set and so
					 * would use it as the current time rather than making a system call.
					 * Note that jnl_file_extend() done below might trigger a jnl_write_epoch_rec()
					 *	due to the fact that it switched journal files.
					 * Therefore, it is imperative we assign gbl_jrec_time before atleast the call to
					 *	jnl_file_extend().
					 * To be safer, we do it the moment we increment early_tn.
					 */
					JNL_SHORT_TIME(gbl_jrec_time);
					jnl_status = jnl_ensure_open();
					if (jnl_status == 0)
					{
						jbp = csa->jnl->jnl_buff;
						TOTAL_NONTPJNL_REC_SIZE(total_jnl_rec_size, non_tp_jfb_ptr, csa, cw_set_depth);
						if ((jbp->freeaddr + total_jnl_rec_size) > jbp->filesize)
						{	/* Moved as part of change to prevent journal records splitting
							 * across multiple generation journal files. */
							jnl_flush(csa->jnl->region);
							if (jnl_file_extend(csa->jnl, total_jnl_rec_size) == -1)
							{
								assert (!JNL_ENABLED(csd));
								status = cdb_sc_jnlclose;
								t_commit_cleanup(status, 0);
								REVERT;
								goto failed;
							}
						}
						if (csa->jnl_before_image != csa->jnl->jnl_buff->before_images)
						{
							status = cdb_sc_jnlstatemod;
							t_commit_cleanup(status, 0);
							REVERT;
							goto failed;
						}
						/* tp_tend sets gbl_jrec_time before calling jnl_put_jrt_pini. t_end doesn't.
						 * see comment in jnl_put_pini.c related to gbl_jrec_time for the reason.
						 */
						if (0 == csa->jnl->pini_addr)
							jnl_put_jrt_pini(csa);
						if (jbp->before_images)
						{
							if (jbp->next_epoch_time <= gbl_jrec_time)
							{	/* Flush the cache. Since we are in crit, defer syncing epoch */
								if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH))
								{
									if (REPL_ENABLED(csd) && cw_depth
											&& (run_time || is_db_updater))
									{
										QWINCRBYDW(jnlpool_ctl->write_addr,
												jnlpool_ctl->jnlpool_size);
											/* to refresh any history */
										rel_lock(jnlpool.jnlpool_dummy_reg);
									}
									SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
									BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_wcsflu);
									status = cdb_sc_cacheprob;
									t_commit_cleanup(status, 0);
									REVERT;
									goto failed;
								}
								VMS_ONLY(
									if (csd->clustered  &&
										!CCP_SEGMENT_STATE(csa->nl,
											CCST_MASK_HAVE_DIRTY_BUFFERS))
									{
										CCP_FID_MSG(gv_cur_region, CCTR_FLUSHLK);
										ccp_userwait(gv_cur_region,
											CCST_MASK_HAVE_DIRTY_BUFFERS, 0,
												 csa->nl->ccp_cycle);
									}
								)
							}
							for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
							{	/* write out before-update journal image records */
								if (gds_t_write_root == cs->mode)
									continue;
								ASSERT_IS_WITHIN_SHM_BOUNDS(cs->old_block, csa);
								DBG_ENSURE_OLD_BLOCK_IS_VALID(cs, is_mm, csa, csd);
								if ((NULL != cs->old_block) &&
									((blk_hdr_ptr_t)(cs->old_block))->tn < jbp->epoch_tn)
								{
									jnl_write_pblk(csa, cs->blk, (blk_hdr_ptr_t)cs->old_block);
									cs->jnl_freeaddr = jbp->freeaddr;
								} else
									cs->jnl_freeaddr = 0;
							}
						}
						if (dse_running)
						{	/* Write after image record for DSE */
							assert(1 == cw_set_depth); /* DSE changes only one block at a time */
							cs = cw_set;
							jnl_write_aimg_rec(csa, cs->blk, (blk_hdr_ptr_t)cs->new_buff);
						} else if (mu_reorg_process || 0 == cw_depth
									|| inctn_gvcstput_extra_blk_split == inctn_opcode)
							/* mupip reorg or gvcst_bmp_mark_free, extra block split in gvcstput */
							jnl_write_inctn_rec(csa);
						else
							jnl_write_logical(csa, non_tp_jfb_ptr);
					} else
						rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
				}
				csa->prev_free_blks = csa->ti->free_blocks;
				csa->t_commit_crit = TRUE;
				for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
				{
					if (gds_t_write_root == cs->mode)
					{
						assert((cs - cw_set) + 1 == cw_depth);
						continue;
					}
					if (is_mm)
					{
						if (cdb_sc_normal != (status = mm_update(cs, cs_top, ctn, ctn, dummysi)))
						{
							assert(FALSE);
							break;
						}
					} else
					{
						if (csd->dsid)
						{
							if (ERR_GVKILLFAIL == t_err)
							{
								if (cs == cw_set)
								{
									if ((gds_t_acquired == cs->mode) ||
									    ((cw_set_depth > 1) && (0 == cw_set[1].level)))
										rc_cpt_inval();
									else
										rc_cpt_entry(cs->blk);
								}
							} else	if (0 == cs->level)
								rc_cpt_entry(cs->blk);
						}
						if (cdb_sc_normal != (status = bg_update(cs, cs_top, ctn, ctn, dummysi)))
						{
							break;
						}
					}
					cs->mode = gds_t_committed;
				}
				csa->t_commit_crit = FALSE;
				if (cdb_sc_normal == status)
				{
					++csa->ti->curr_tn;
					assert(csa->ti->curr_tn == csa->ti->early_tn);
					    /* write out the db header every HEADER_UPDATE_COUNT -1 transactions */
					if (!(csa->ti->curr_tn & (HEADER_UPDATE_COUNT-1)))
						fileheader_sync(gv_cur_region);
				}
				if (REPL_ENABLED(csd) && cw_depth && (run_time || is_db_updater)
						&& inctn_invalid_op == inctn_opcode)
				{
					assert(QWGT(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr));
					QWINCRBY(temp_jnlpool_ctl->jnl_seqno, seq_num_one);
					QWASSIGN(csa->hdr->reg_seqno, temp_jnlpool_ctl->jnl_seqno);
					if (is_updproc)
					{
						QWINCRBY(max_resync_seqno, seq_num_one);
						QWASSIGN(csa->hdr->resync_seqno, max_resync_seqno);
					}
					assert(cumul_jnl_rec_len == (temp_jnlpool_ctl->write - jnlpool_ctl->write +
						(temp_jnlpool_ctl->write > jnlpool_ctl->write ? 0 : jnlpool_ctl->jnlpool_size)));
					/* the following statements should be atomic */
					jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jnlpool_ctl->write);
					jnl_header->jnldata_len = cumul_jnl_rec_len;
					jnl_header->prev_jnldata_len = jnlpool_ctl->lastwrite_len;
					/* The following assert should be an == rather than a >= (as in tp_tend) because, we have
					 * either one or no update.  If no update, we would have no cw_depth and we wouldn't enter
					 * this path.  If there is an update, then both the indices should be 1.
					 */
					INT8_ONLY(assert(cumul_index == cu_jnl_index);)
					jnlpool_ctl->lastwrite_len = jnl_header->jnldata_len;
					QWINCRBYDW(jnlpool_ctl->write_addr, jnl_header->jnldata_len);
					jnlpool_ctl->write = temp_jnlpool_ctl->write;
					jnlpool_ctl->jnl_seqno = temp_jnlpool_ctl->jnl_seqno;
					rel_lock(jnlpool.jnlpool_dummy_reg);
				}
			}
		}
		if (cdb_sc_normal == status)
		{
			save_early_tn = csa->ti->early_tn;
			while (cr_array_index)
				cr_array[--cr_array_index]->in_cw_set = FALSE;
			if (need_kip_incr)		/* increment kill_in_prog */
			{
				INCR_KIP(cs_data, cs_addrs, kip_incremented);
				need_kip_incr = FALSE;
			}
			rel_crit(gv_cur_region);
			if (block_saved)
				backup_buffer_flush(gv_cur_region);
			UNIX_ONLY(
				if (unhandled_stale_timer_pop)
					process_deferred_stale();
			)
			if (0 != cw_set_depth)
			{
				wcs_timer_start(gv_cur_region, TRUE);
				if (REPL_ENABLED(csd) && dse_running)
					send_msg(VARLSTCNT(5) ERR_NOTREPLICATED, 4, start_ctn + 1, LEN_AND_LIT("DSE"), process_id);
			}
		} else if (cdb_sc_cacheprob != status)
			t_commit_cleanup(status, 0);
		else
			status = cdb_sc_normal;
	}
	REVERT;
	if (cdb_sc_normal == status)
		return ctn;
/*** Warning: Possible fall-thru... ***/
  failed:
	gv_target->clue.end = 0;
	if ((CDB_STAGNATE - 1) > t_tries)
		rel_crit(gv_cur_region);
	t_retry(status);
	cw_map_depth = 0;
	return 0;
}
