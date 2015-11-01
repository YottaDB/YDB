/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>
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
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#ifdef UNIX
#include "process_deferred_stale.h"
#endif
#include "t_end.h"
#include "add_inter.h"
#include "jnl_write_pblk.h"
#include "jnl_write_aimg_rec.h"
#include "memcoherency.h"

#define BLOCK_FLUSHING(x) (csa->hdr->clustered && x->flushing && !CCP_SEGMENT_STATE(cs_addrs->nl,CCST_MASK_HAVE_DIRTY_BUFFERS))

#define	RESTORE_CURRTN_IF_NEEDED(csa, write_inctn, decremented_currtn)					\
{													\
	if (write_inctn && decremented_currtn)								\
	{	/* decremented curr_tn above; need to restore to original state due to the restart */	\
		assert(csa->now_crit);									\
		if (csa->now_crit)									\
		{	/* need crit to update curr_tn and early_tn */					\
			csa->ti->curr_tn++;								\
			csa->ti->early_tn++;								\
		}											\
		decremented_currtn = FALSE;								\
	}												\
}

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
GBLREF bool			is_standalone;
GBLREF boolean_t		is_updproc;
GBLREF seq_num			seq_num_one;
GBLREF boolean_t		mu_reorg_process;
GBLREF boolean_t		dse_running;
GBLREF boolean_t		unhandled_stale_timer_pop;
GBLREF jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF inctn_opcode_t		inctn_opcode;
GBLREF boolean_t 		kip_incremented;
GBLREF boolean_t		need_kip_incr;
GBLREF boolean_t		write_after_image;
GBLREF boolean_t		is_replicator;
GBLREF seq_num			seq_num_zero;
GBLREF jnl_gbls_t		jgbl;
GBLREF jnl_fence_control	jnl_fence_ctl;

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
	bt_rec_ptr_t		bt;
	bool			blk_used;
	cache_rec_ptr_t		cr;
	cw_set_element		*cs, *cs_top;
	enum cdb_sc		status;
	int			int_depth;
	uint4			jnl_status;
	jnl_buffer_ptr_t	jbp;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sgm_info		*dummysi = NULL;	/* needed as a dummy parameter for {mm,bg}_update */
	srch_blk_status		*t1;
	trans_num		start_ctn, valid_thru, ctn, tnque_earliest_tn;
	unsigned char		cw_depth;
	jnldata_hdr_ptr_t	jnl_header;
	uint4			total_jnl_rec_size, tmp_cumul_jnl_rec_len, tmp_cw_set_depth;
	uint4			tot_jrec_size;
	jnlpool_ctl_ptr_t	jpl, tjpl;
	boolean_t		replication = FALSE;
	boolean_t		is_mm, release_crit = FALSE;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress
						    * This is used to read before-images of blocks whose cs->mode is gds_t_create */
	boolean_t		write_inctn = FALSE;	/* set to TRUE in case writing an inctn record is necessary */
	boolean_t		decremented_currtn;

	error_def(ERR_GVKILLFAIL);
	error_def(ERR_NOTREPLICATED);
	error_def(ERR_JNLTRANS2BIG);

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
	if (0 != cw_depth)
	{	/* Caution : since csa->backup_in_prog and read_before_image are initialized below only if (cw_depth),
		 * 	these variable should be used below only within an if (cw_depth).
		 */
		csa->backup_in_prog = (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb);
		read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image) || csa->backup_in_prog);
		for (cs = &cw_set[0], cs_top = cs + cw_depth; cs < cs_top; cs++)
		{
			assert(0 == cs->jnl_freeaddr);	/* ensure haven't missed out resetting jnl_freeaddr for any cse in
							 * t_write/t_create/t_write_map/t_write_root/mu_write_map [D9B11-001991] */
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
				if (!blk_used || !read_before_image)
					cs->old_block = NULL;
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
	if (JNL_ENABLED(csa))
	{	/* compute the total journal record size requirements before grab_crit().
		 * there is code later that will check for state changes from now to then and if so do a recomputation
		 */
		assert(!cw_map_depth || cw_set_depth < cw_map_depth);
		tmp_cw_set_depth = cw_map_depth ? cw_map_depth : cw_set_depth;
		TOTAL_NONTPJNL_REC_SIZE(total_jnl_rec_size, non_tp_jfb_ptr, csa, tmp_cw_set_depth);
		/* compute current transaction's maximum journal space needs in number of disk blocks */
		tot_jrec_size = MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size);
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
					   && !wcs_get_space(gv_cur_region, cw_set_depth + 1, NULL))
					assert(FALSE);	/* wcs_get_space() should have returned TRUE unconditionally in this case */
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
		/* Any retry transition where the destination state is the 3rd retry, we don't want to release crit,
		 * i.e. for 2nd to 3rd retry transition or 3rd to 3rd retry transition.
		 * Therefore we need to release crit only if (CDB_STAGNATE - 1) > t_tries
		 * But 2nd to 3rd retry transition doesn't occur if in 2nd retry we get jnlstatemod/jnlclose/backupstatemod code.
		 * Hence the variable release_crit to track the above.
		 */
		release_crit = (CDB_STAGNATE - 1) > t_tries;
		if (JNL_ALLOWED(csa))
		{
			if ((csa->jnl_state != csd->jnl_state) || (csa->jnl_before_image != csd->jnl_before_image))
			{ 	/* csd->jnl_state or csd->jnl_before_image changed since last time
				 * 	csa->jnl_before_image and csa->jnl_state got set */
				csa->jnl_before_image = csd->jnl_before_image;
				csa->jnl_state = csd->jnl_state;
				/* jnl_file_lost() causes a jnl_state transition from jnl_open to jnl_closed
				 * and additionally causes a repl_state transition from repl_open to repl_closed
				 * all without standalone access. This means that csa->repl_state might be repl_open
				 * while csd->repl_state might be repl_closed. update csa->repl_state in this case
				 * as otherwise the rest of the code might look at csa->repl_state and incorrectly
				 * conclude replication is on and generate sequence numbers when actually no journal
				 * records are being generated. [C9D01-002219]
				 */
				csa->repl_state = csd->repl_state;
				status = cdb_sc_jnlstatemod;
				if ((CDB_STAGNATE - 1) == t_tries)
					release_crit = TRUE;
				REVERT;
				goto failed;
			}
			if (JNL_ENABLED(csa))
			{	/* check if current transaction's journal size needs are greater than max jnl file size */
				if (tot_jrec_size > csd->autoswitchlimit)
				{	/* can't fit in current transaction's journal records into one journal file */
					rts_error(VARLSTCNT(6) ERR_JNLTRANS2BIG, 4, tot_jrec_size,
							JNL_LEN_STR(csd), csd->autoswitchlimit);
				}
			}
		}
		if (cw_depth && (csa->backup_in_prog != (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb)))
		{
			if (!csa->backup_in_prog && !(JNL_ENABLED(csa) && csa->jnl_before_image))
			{	/* If online backup is in progress now and before-image journaling is not enabled,
				 * we would not have read before-images for created blocks. Although it is possible
				 * that this transaction might not have blocks with gds_t_create at all, we expect
				 * this backup_in_prog state change to be so rare that it is ok to restart.
				 */
				status = cdb_sc_backupstatemod;
				if ((CDB_STAGNATE - 1) == t_tries)
					release_crit = TRUE;
				REVERT;
				goto failed;
			}
			csa->backup_in_prog = !csa->backup_in_prog;	/* reset csa->backup_in_prog to current state */
			read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image) || csa->backup_in_prog); /* recalculate */
		}
		/* in crit, ensure cache-space is available. the out-of-crit check done above might not have been enough */
		if (!is_mm && update_trans && (csa->nl->wc_in_free < (int4)(cw_set_depth + 1))
			   && !wcs_get_space(gv_cur_region, cw_set_depth + 1, NULL))
		{
			assert(FALSE);
			SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
			status = cdb_sc_cacheprob;
			if ((CDB_STAGNATE - 1) == t_tries)
				release_crit = TRUE;
			REVERT;
			goto failed;
		}
		assert(csd == csa->hdr);
		if (inctn_invalid_op != inctn_opcode)
		{
			assert(cw_set_depth || mu_reorg_process);
			write_inctn = TRUE;	/* mupip reorg or gvcst_bmp_mark_free or extra block split in gvcstput */
			decremented_currtn = FALSE;
			if (jgbl.forw_phase_recovery && !JNL_ENABLED(csa))
			{	/* forward recovery (deduced above from the fact that journaling is not enabled) is supposed
				 * to accurately simulate GT.M runtime activity for every transaction number. The way it does
				 * this is by incrementing transaction numbers for all inctn records that GT.M wrote and not
				 * incrementing transaction number for any inctn activity that forward recovery internally needs
				 * to do. all inctn activity done outside of t_end has already been protected against incrementing
				 * transaction number in case of forward recovery. t_end is a little bit tricky since in this case
				 * a few database blocks get modified with the current transaction number and not incrementing the
				 * transaction number might result in the database transaction number being lesser than the block
				 * transaction number. we work around this problem by decrementing the database transaction number
				 * just before the commit so the database block updates for the inctn transaction get the
				 * transaction number of the previous transaction effectively merging the inctn transaction with
				 * the previous transaction.
				 */
				assert((inctn_gvcstput_extra_blk_split == inctn_opcode) || (1 == cw_set_depth));
				csa->ti->curr_tn--;
				csa->ti->early_tn--;
				decremented_currtn = TRUE;
			}
		}
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
								assert(FALSE);
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch1);
								status = cdb_sc_crbtmismatch;
								break;
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
						if ((NULL != cr) && (NULL != bt) && (cr->blk != bt->blk))
						{
							assert(FALSE);
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch2);
							status = cdb_sc_crbtmismatch;
							break;
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
				assert(0 == cs->jnl_freeaddr);	/* ensure haven't missed out resetting jnl_freeaddr for any cse in
								 * t_write/t_create/{t,mu}_write_map/t_write_root [D9B11-001991] */
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
								assert(FALSE);
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch3);
								status = cdb_sc_crbtmismatch;
								break;
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
			if (cw_depth && read_before_image && !is_mm)
			{
				for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
				{	/* have already read old block for creates before we got crit, make sure
					 * cache record still has correct block. if not, reset "cse" fields to
					 * point to correct cache-record. this is ok to do since we only need the
					 * prior content of the block (for online backup or before-image journaling)
					 * and did not rely on it for constructing the transaction. Restart if
					 * block is not present in cache now or is being read in currently.
					 */
					if ((gds_t_acquired == cs->mode) && (NULL != cs->old_block))
					{
						assert(read_before_image == ((JNL_ENABLED(csa) && csa->jnl_before_image)
											|| csa->backup_in_prog));
						cr = db_csh_get(cs->blk);
						if ((cache_rec_ptr_t)CR_NOTVALID == cr)
						{
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_cwset);
							status = cdb_sc_cacheprob;
							break;
						}
						if ((NULL == cr) || (0 <= cr->read_in_progress))
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_lostbefor;
							break;
						}
						cr_array[cr_array_index++] = cr;
						assert(FALSE == cr->in_cw_set);
						cr->in_cw_set = TRUE;
						cr->refer = TRUE;
						cs->old_block = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
						cs->cr = cr;
						cs->cycle = cr->cycle;
					}
				}
			}
			if (cdb_sc_normal == status)
			{
				if (REPL_ENABLED(csa) && cw_depth && is_replicator && (inctn_invalid_op == inctn_opcode))
				{
					replication = TRUE;
					jpl = jnlpool_ctl;
					tjpl = temp_jnlpool_ctl;
					grab_lock(jnlpool.jnlpool_dummy_reg);
					QWASSIGN(tjpl->write_addr, jpl->write_addr);
					QWASSIGN(tjpl->write, jpl->write);
					QWASSIGN(tjpl->jnl_seqno, jpl->jnl_seqno);
					INT8_ONLY(assert(tjpl->write == tjpl->write_addr % tjpl->jnlpool_size);)
					tmp_cumul_jnl_rec_len = jgbl.cumul_jnl_rec_len + sizeof(jnldata_hdr_struct);
					tjpl->write += sizeof(jnldata_hdr_struct);
					if (tjpl->write >= tjpl->jnlpool_size)
					{
						assert(tjpl->write == tjpl->jnlpool_size);
						tjpl->write = 0;
					}
					assert(QWEQ(jpl->early_write_addr, jpl->write_addr));
					QWADDDW(jpl->early_write_addr, jpl->write_addr, tmp_cumul_jnl_rec_len);
					/* Source server does not read in crit. It relies on early_write_addr, the transaction
					 * data, lastwrite_len, write_addr being updated in that order. To ensure this order,
					 * we have to force out early_write_addr to its coherency point now. If not, the source
					 * server may read data that is overwritten (or stale). This is true only on
					 * architectures and OSes that allow unordered memory access
					 */
					COMMIT_SHM_UPDATES;
				}
				assert(cw_set_depth < CDB_CW_SET_SIZE);
				assert(csa->ti->early_tn == csa->ti->curr_tn);
				start_ctn = csa->ti->curr_tn;
				csa->ti->early_tn = start_ctn + 1;
				if (JNL_ENABLED(csa))
				{
 					/* The JNL_SHORT_TIME done below should be done before any journal writing activity on the
 					 * journal file. This is because all the jnl record writing routines assume that
					 * jgbl.gbl_jrec_time is initialized approporiately.
					 * Note that jnl_ensure_open() can call cre_jnl_file() which
					 * in turn assumes jgbl.gbl_jrec_time is set. Also jnl_file_extend() can call
					 * jnl_write_epoch_rec() which in turn assumes jgbl.gbl_jrec_time is set.
					 * In case of forw-phase-recovery, mur_output_record() would have already set this.
					 */
					if (!jgbl.forw_phase_recovery)
						JNL_SHORT_TIME(jgbl.gbl_jrec_time);
					assert(jgbl.gbl_jrec_time);
					jnl_status = jnl_ensure_open();
					assert((csa->jnl_state == csd->jnl_state) &&
						(csa->jnl_before_image == csd->jnl_before_image));
					if (jnl_status == 0)
					{
						jbp = csa->jnl->jnl_buff;
						/* tmp_cw_set_depth was used to do TOTAL_NONTPJNL_REC_SIZE calculation;
						 * ensure it has not changed until now when the actual jnl record write occurs.
						 * same case with csa->jnl_before_images & jbp->before_images.
						 */
						assert(cw_set_depth == tmp_cw_set_depth);
						assert(jbp->before_images == csa->jnl_before_image);
						if (DISK_BLOCKS_SUM(jbp->freeaddr, total_jnl_rec_size) > jbp->filesize)
						{	/* Moved as part of change to prevent journal records splitting
							 * across multiple generation journal files. */
							jnl_flush(csa->jnl->region);
							if (jnl_file_extend(csa->jnl, total_jnl_rec_size) == -1)
							{
								assert((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa)));
								status = cdb_sc_jnlclose;
								RESTORE_CURRTN_IF_NEEDED(csa, write_inctn, decremented_currtn);
								t_commit_cleanup(status, 0);
								if ((CDB_STAGNATE - 1) == t_tries)
									release_crit = TRUE;
								REVERT;
								goto failed;
							}
						}
						if (0 == csa->jnl->pini_addr)
							jnl_put_jrt_pini(csa);
						if (jbp->before_images)
						{
							if (jbp->next_epoch_time <= jgbl.gbl_jrec_time)
							{	/* Flush the cache. Since we are in crit, defer syncing epoch */
								if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH))
								{
									SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
									BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_wcsflu);
									status = cdb_sc_cacheprob;
									RESTORE_CURRTN_IF_NEEDED(csa, write_inctn,
													decremented_currtn);
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
						if (dse_running || write_after_image)
						{	/* Write after image record for DSE */
							assert(write_after_image || 1 == cw_set_depth); /* DSE changes only one
													block at a time */
							cs = cw_set;
							jnl_write_aimg_rec(csa, cs->blk, (blk_hdr_ptr_t)cs->new_buff);
						} else if (write_inctn)
							jnl_write_inctn_rec(csa);
						else if (0 == jnl_fence_ctl.level)
						{
							if (replication)
								QWASSIGN(jnl_fence_ctl.token, tjpl->jnl_seqno);
							else
								QWASSIGN(jnl_fence_ctl.token, seq_num_zero);
							jnl_write_logical(csa, non_tp_jfb_ptr);
						} else
							jnl_write_ztp_logical(csa, non_tp_jfb_ptr);
					} else
					{
						csa->ti->early_tn = start_ctn;
						rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
					}
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
							assert(FALSE);
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
				if (replication)
				{
					assert(QWGT(jpl->early_write_addr, jpl->write_addr));
					QWINCRBY(tjpl->jnl_seqno, seq_num_one);
					QWASSIGN(csa->hdr->reg_seqno, tjpl->jnl_seqno);
					if (is_updproc)
					{
						QWINCRBY(jgbl.max_resync_seqno, seq_num_one);
						QWASSIGN(csa->hdr->resync_seqno, jgbl.max_resync_seqno);
					}
					assert(tmp_cumul_jnl_rec_len == (tjpl->write - jpl->write +
						(tjpl->write > jpl->write ? 0 : jpl->jnlpool_size)));
					/* the following statements should be atomic */
					jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jpl->write);
					jnl_header->jnldata_len = tmp_cumul_jnl_rec_len;
					jnl_header->prev_jnldata_len = jpl->lastwrite_len;
					/* The following assert should be an == rather than a >= (as in tp_tend) because, we have
					 * either one or no update.  If no update, we would have no cw_depth and we wouldn't enter
					 * this path.  If there is an update, then both the indices should be 1.
					 */
					INT8_ONLY(assert(jgbl.cumul_index == jgbl.cu_jnl_index);)
					jpl->lastwrite_len = jnl_header->jnldata_len;
					/* For systems with UNORDERED memory access (example, ALPHA), on a multi processor system,
					 * it is possible that the source server notices the change in write_addr
					 * before seeing the change to jnlheader->jnldata_len, leading it to read an invalid
					 * transaction length. To avoid such conditions, we should commit the order of shared
					 * memory updates before we update write_addr. This ensures that the source server sees all
					 * shared memory updates related to a transaction before the change in write_addr
		 			 *
		 			 * Read Alpha Architecture Reference Manual, edited by Richard L Sites, Chapter "System
		 			 * Architecture and Programming Implications" for memory coherency issues.
					 */
					COMMIT_SHM_UPDATES;
					QWINCRBYDW(jpl->write_addr, jnl_header->jnldata_len);
					jpl->write = tjpl->write;
					jpl->jnl_seqno = tjpl->jnl_seqno;
					rel_lock(jnlpool.jnlpool_dummy_reg);
				}
			}
		}
		if (cdb_sc_normal == status)
		{
			while (cr_array_index)
				cr_array[--cr_array_index]->in_cw_set = FALSE;
			if (need_kip_incr)		/* increment kill_in_prog */
			{
				INCR_KIP(csd, csa, kip_incremented);
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
				if (REPL_ENABLED(csa) && dse_running)
					send_msg(VARLSTCNT(5) ERR_NOTREPLICATED, 4, start_ctn + 1, LEN_AND_LIT("DSE"), process_id);
			}
		} else if (cdb_sc_cacheprob != status)
		{
			RESTORE_CURRTN_IF_NEEDED(csa, write_inctn, decremented_currtn);
			t_commit_cleanup(status, 0);
		} else
			status = cdb_sc_normal;
	}
	REVERT;
	if (cdb_sc_normal == status)
		return ctn;
/*** Warning: Possible fall-thru... ***/
  failed:
	gv_target->clue.end = 0;
	RESTORE_CURRTN_IF_NEEDED(csa, write_inctn, decremented_currtn);
	if (release_crit && (csa->now_crit))
		rel_crit(gv_cur_region);
	t_retry(status);
	cw_map_depth = 0;
	return 0;
}
