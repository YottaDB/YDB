/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cdb_sc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "gdscc.h"
#include "copy.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "rc_oflow.h"
#include "repl_msg.h"
#include "gtmsource.h"

/* Include prototypes */
#include "t_write.h"
#include "t_write_root.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "t_create.h"
#include "gvcst_blk_build.h"
#include "gvcst_expand_key.h"
#include "gvcst_protos.h"	/* for gvcst_search,gvcst_search_blk,gvcst_put prototype */
#include "op.h"			/* for op_add prototype */
#include "format_targ_key.h"	/* for format_targ_key prototype */

GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	gv_key			*gv_altkey, *gv_currkey;
GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	short			dollar_tlevel;
GBLREF	uint4			process_id;
GBLREF	cw_set_element		cw_set[];
GBLREF	unsigned char		cw_set_depth;
GBLREF 	unsigned int		t_tries;
GBLREF	boolean_t		pool_init;
GBLREF	boolean_t		horiz_growth;
GBLREF	int4			prev_first_off, prev_next_off;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	boolean_t		is_updproc;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	int			gv_fillfactor,
                                rc_set_fragment;	/* Contains offset within data at which data fragment starts */
GBLREF	uint4			update_array_size, cumul_update_array_size;	/* the current total size of the update array */
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnl_format_buffer       *non_tp_jfb_ptr;
GBLREF	inctn_opcode_t          inctn_opcode;
GBLREF	boolean_t		is_replicator;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	boolean_t		in_gvcst_incr;
GBLREF	mval			*post_incr_mval;
GBLREF	boolean_t		is_dollar_incr;
GBLREF	int4			update_trans;
GBLREF	boolean_t		pre_incr_update_trans;	/* copy of "sgm_info_ptr->update_trans" before the $INCR */

#ifdef DEBUG
GBLREF	boolean_t		skip_block_chain_tail_check;
#endif

#define	ENSURE_VALUE_WITHIN_MAX_REC_SIZE(value)									\
{														\
	if (gv_currkey->end + 1 + value.len + sizeof(rec_hdr) > gv_cur_region->max_rec_size)			\
	{                                                                                                       \
		if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))			\
			end = &buff[MAX_ZWR_KEY_SZ - 1];                                                        \
		rts_error(VARLSTCNT(10) ERR_REC2BIG, 4, gv_currkey->end + 1  + value.len + sizeof(rec_hdr),	\
			  (int4)gv_cur_region->max_rec_size,                                                    \
			  REG_LEN_STR(gv_cur_region), ERR_GVIS, 2, end - buff, buff);				\
	}                                                                                                       \
}

#define	ISSUE_RSVDBYTE2HIGH_ERROR								\
{												\
	/* The record that is newly inserted/updated does not fit by itself in a separate block	\
	 * if the current reserved-bytes for this database is taken into account. Cannot go on.	\
	 */											\
	if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, temp_key, TRUE)))			\
		end = &buff[MAX_ZWR_KEY_SZ - 1];						\
	rts_error(VARLSTCNT(11) ERR_RSVDBYTE2HIGH, 5, new_blk_size_single,			\
		REG_LEN_STR(gv_cur_region), blk_size, reserved_bytes,				\
		ERR_GVIS, 2, end - buff, buff);							\
}

static	block_id	lcl_root;
static	int4		blk_size, blk_fill_size;
static	int4 const	zeroes = 0;
static	boolean_t	jnl_format_done;

static	boolean_t	gvcst_put_blk(mval *val, boolean_t *extra_block_split_req);

void	gvcst_put(mval *val)
{
	boolean_t	extra_block_split_req;
	sm_uc_ptr_t	jnlpool_instname;

	error_def(ERR_SCNDDBNOUPD);
	error_def(ERR_REPLINSTMISMTCH);

	is_dollar_incr = in_gvcst_incr;
	in_gvcst_incr = FALSE;
	if (REPL_ENABLED(cs_data) && is_replicator)
	{
		if (FALSE == pool_init)
			jnlpool_init((jnlpool_user)GTMPROC, (boolean_t)FALSE, (boolean_t *)NULL);
		assert(pool_init);
		if (!cs_addrs->replinst_matches_db)
		{
			if (jnlpool_ctl->upd_disabled && !is_updproc)
				rts_error(VARLSTCNT(1) ERR_SCNDDBNOUPD);
			UNIX_ONLY(jnlpool_instname = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.instname;)
			VMS_ONLY(jnlpool_instname = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.gtmgbldir;)
			if (STRCMP(cs_addrs->nl->replinstname, jnlpool_instname))
				rts_error(VARLSTCNT(8) ERR_REPLINSTMISMTCH, 6, LEN_AND_STR(jnlpool_instname),
					DB_LEN_STR(gv_cur_region), LEN_AND_STR(cs_addrs->nl->replinstname));
			cs_addrs->replinst_matches_db = TRUE;
		}
	}
	blk_size = cs_data->blk_size;
	blk_fill_size = (blk_size * gv_fillfactor) / 100 - cs_data->reserved_bytes;
	jnl_format_done = FALSE;	/* do jnl_format() only once per logical transaction irrespective of number of retries */
	do
	{
		T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVPUTFAIL);
		lcl_root = gv_target->root;
		while(!gvcst_put_blk(val, &extra_block_split_req))
			;
	} while (extra_block_split_req);
}

static	boolean_t gvcst_put_blk(mval *val, boolean_t *extra_block_split_req)
{
	blk_segment		*bs1, *bs_ptr, *new_blk_bs;
	block_id		allocation_clue, tp_root, gvt_for_root;
	block_index		left_hand_index, ins_chain_index, next_blk_index;
	block_offset		next_offset, first_offset, ins_off1, ins_off2, old_curr_chain_next_off;
	cw_set_element		*cse, *cse_new, *old_cse;
	gv_namehead		*save_targ;
	enum cdb_sc		status;
	gv_key			*temp_key;
	mstr			value;
	off_chain		chain1, curr_chain, prev_chain;
	rec_hdr_ptr_t		curr_rec_hdr, extra_rec_hdr, next_rec_hdr, new_star_hdr, rp;
	srch_blk_status		*bh, *bq, *tp_srch_status;
	srch_hist		*dir_hist;
	int			cur_blk_size, blk_seg_cnt, delta, i, left_hand_offset, n, ins_chain_offset,
				new_blk_size_l, new_blk_size_r, new_blk_size_single, new_blk_size, blk_reserved_size,
				last_possible_left_offset, new_rec_size, next_rec_shrink, next_rec_shrink1,
				offset_sum, rec_cmpc, target_key_size, tp_lev, undo_index;
	uint4			segment_update_array_size;
	int4			reserved_bytes;
	char			*va;
	sm_uc_ptr_t		cp1, cp2, curr;
	unsigned short		extra_record_orig_size, rec_size, temp_short;
	unsigned int		prev_rec_offset, prev_rec_match, curr_rec_offset, curr_rec_match;
	bool			chain_in_orig_block, copy_extra_record, level_0, new_rec, no_pointers,
				succeeded = FALSE;
	boolean_t		make_it_null, gbl_target_was_set, duplicate_set = FALSE, new_rec_goes_to_right;
	key_cum_value		*tempkv;
	jnl_format_buffer	*jfb;
	jnl_action		*ja;
	mval			*set_val;	/* actual right-hand-side value of the SET or $INCR command */
	ht_ent_int4		*tabent;
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end;

	error_def(ERR_GVINCRISOLATION);
	error_def(ERR_GVIS);
	error_def(ERR_REC2BIG);
	error_def(ERR_RSVDBYTE2HIGH);

	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	/* When the following two asserts trip, we should change the data types of prev_first_off
	 * and prev_next_off, so they satisfy the assert.
	 */
	assert(sizeof(prev_first_off) >= sizeof(block_offset));
	assert(sizeof(prev_next_off) >= sizeof(block_offset));
	prev_first_off = prev_next_off = PREV_OFF_INVALID;
	horiz_growth = FALSE;
	/* this needs to be initialized before any code that does a "goto retry" since this gets used there */
	save_targ = gv_target;
	if (INVALID_GV_TARGET != reset_gv_target)
		gbl_target_was_set = TRUE;
	else
	{
		gbl_target_was_set = FALSE;
		reset_gv_target = save_targ;
	}
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	/* Assume we don't require an additional block split */
	*extra_block_split_req = FALSE;
	/* level_0 == true and no_pointers == false means that this is a directory tree data block containing pointers to roots */
	level_0 = no_pointers = TRUE;
	assert(gv_altkey->top == gv_currkey->top);
	assert(gv_altkey->top == gv_keysize);
	assert(gv_currkey->end < gv_currkey->top);
	assert(gv_altkey->end < gv_altkey->top);
	temp_key = gv_currkey;
	dir_hist = NULL;
	ins_chain_index = 0;
	gv_target->root = lcl_root;
	tp_root = lcl_root;
	if (0 == dollar_tlevel)
		update_array_ptr = update_array;
	else
	{
		segment_update_array_size = UA_NON_BM_SIZE(cs_data);
		ENSURE_UPDATE_ARRAY_SPACE(segment_update_array_size);
		curr_chain = *(off_chain *)&gv_target->root;
		if (curr_chain.flag == 1)
		{
			tp_get_cw(sgm_info_ptr->first_cw_set, (int)curr_chain.cw_index, &cse);
			tp_root = cse->blk;
		}
	}
	/* the below "if (0 == tp_root)" check seems unnecessary given a similar if check following it --- nars -- 2004/05/26 */
	if (0 == tp_root)
	{	/* the process may have just killed the global; that action creates a new root and zeroes gv_target->root,
		 * so search the GVT for a new root to cover that case */
		gv_target = cs_addrs->dir_tree;
		for (cp1 = temp_key->base, cp2 = gv_altkey->base;  0 != *cp1;)
			*cp2++ = *cp1++;
		*cp2++ = 0;
		*cp2 = 0;
		gv_altkey->end = cp2 - gv_altkey->base;
		assert(gv_altkey->end <= gv_altkey->top);
		dir_hist = &gv_target->hist;
		status = gvcst_search(gv_altkey, NULL);
		gv_target->clue.end = 0;
		RESET_GV_TARGET_LCL(save_targ);
		if (cdb_sc_normal != status)
			goto retry;
		if (gv_altkey->end + 1 == dir_hist->h[0].curr_rec.match)
		{
			GET_LONG(tp_root, (dir_hist->h[0].buffaddr + sizeof(rec_hdr)
					   + dir_hist->h[0].curr_rec.offset + gv_altkey->end + 1
					   - ((rec_hdr_ptr_t)(dir_hist->h[0].buffaddr + dir_hist->h[0].curr_rec.offset))->cmpc));
			if (0 < dollar_tlevel)
			{
				gvt_for_root = dir_hist->h[0].blk_num;
				curr_chain = *(off_chain *)&gvt_for_root;
				if (curr_chain.flag == 1)
					tp_get_cw(sgm_info_ptr->first_cw_set, curr_chain.cw_index, &cse);
				else
				{
					if (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use,
												(uint4 *)&gvt_for_root)))
						tp_srch_status = tabent->value;
					else
						tp_srch_status = NULL;
					cse = tp_srch_status ? tp_srch_status->ptr : NULL;
				}
				assert(!cse || !cse->high_tlevel);
			}
			assert(0 == gv_target->root);
			gv_target->root = tp_root;
		}
	}
	reserved_bytes = cs_data->reserved_bytes;
	blk_reserved_size = blk_size - reserved_bytes;
	if (0 == tp_root)
	{	/* there is no entry in the GVT (and no root), so create a new empty tree and put the name in the GVT */
		/* Create the data block */
		if (is_dollar_incr)
		{	/* The global variable that is being $INCREMENTed does not exist.
			 * $INCREMENT() should not signal UNDEF error but proceed with an implicit $GET().
			 */
			if (0 == dollar_tlevel)
				update_trans = TRUE;
			else
				sgm_info_ptr->update_trans = TRUE;
			*post_incr_mval = *val;
			MV_FORCE_STR(post_incr_mval);
			value = post_incr_mval->str;
			/* the MAX_REC_SIZE check could not be done in op_gvincr (like is done in op_gvput) because
			 * the post-increment value is not known until here. so do the check here.
			 */
			ENSURE_VALUE_WITHIN_MAX_REC_SIZE(value);
		} else
			value = val->str;
		/* Potential size of a GVT leaf block containing just the new/updated record */
		new_blk_size_single = sizeof(blk_hdr) + sizeof(rec_hdr) + temp_key->end + 1 + value.len;
		if (new_blk_size_single > blk_reserved_size)
		{	/* The record that is newly inserted/updated does not fit by itself in a separate block
			 * if the current reserved-bytes for this database is taken into account. Cannot go on.
			 */
			ISSUE_RSVDBYTE2HIGH_ERROR;
		}
		BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
		curr_rec_hdr->rsiz = sizeof(rec_hdr) + temp_key->end + 1 + value.len;
		curr_rec_hdr->cmpc = 0;
		BLK_INIT(bs_ptr, new_blk_bs);
		BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
		BLK_ADDR(cp1, temp_key->end + 1, unsigned char);
		memcpy(cp1, temp_key->base, temp_key->end + 1);
		BLK_SEG(bs_ptr, cp1, temp_key->end + 1);
		if (0 != value.len)
		{
			BLK_ADDR(va, value.len, char);
			memcpy(va, value.addr, value.len);
			BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
		}
		if (0 == BLK_FINI(bs_ptr, new_blk_bs))
		{
			assert(CDB_STAGNATE > t_tries);
			status = cdb_sc_mkblk;
			goto retry;
		}
		assert(new_blk_bs[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
		/* Create the index block */
		BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
		curr_rec_hdr->rsiz = BSTAR_REC_SIZE;
		curr_rec_hdr->cmpc = 0;
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
		BLK_SEG(bs_ptr, (unsigned char *)&zeroes, sizeof(block_id));
		if (0 == BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			status = cdb_sc_mkblk;
			goto retry;
		}
		assert(bs1[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
		allocation_clue = cs_data->trans_hist.total_blks / 64 + 8; /* roger 19990607 - arbitrary & should be improved */
		next_blk_index = t_create(allocation_clue, (uchar_ptr_t)new_blk_bs, 0, 0, 0);
		++allocation_clue;
		ins_chain_index = t_create(allocation_clue, (uchar_ptr_t)bs1, sizeof(blk_hdr) + sizeof(rec_hdr), next_blk_index, 1);
		temp_key = gv_altkey;
		gv_target->hist.h[0].blk_num = HIST_TERMINATOR;
		gv_target = cs_addrs->dir_tree;
		value.len = sizeof(block_id);
		value.addr = (char *)&zeroes;
		no_pointers = FALSE;
	} else
	{
		if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
			goto retry;
		if (is_dollar_incr)
		{
			target_key_size = gv_currkey->end + 1;
			bh = &gv_target->hist.h[0];
			if (target_key_size == bh->curr_rec.match)
			{	/* $INCR is being done on an existing global variable key in the database.
				 * the value to set the key to has to be determined by adding the existing value
				 * with the increment passed as the input parameter "val" (of type (mval *)) to gvcst_put
				 */
				if (cdb_sc_normal != (status = gvincr_compute_post_incr(bh)))
				{
					assert(CDB_STAGNATE > t_tries);
					goto retry;
				}
			} else
			{	/* The global variable that is being $INCREMENTed does not exist.  $INCREMENT() should not
				 * signal UNDEF error but proceed with an implicit $GET() */
				*post_incr_mval = *val;
				MV_FORCE_STR(post_incr_mval);
			}
			assert(MV_IS_STRING(post_incr_mval));
			/* reset update_trans to TRUE in case it got reset to FALSE by the undef check in previous retry */
			if (0 == dollar_tlevel)
				update_trans = TRUE;
			else
				sgm_info_ptr->update_trans = TRUE;
			value = post_incr_mval->str;
			/* the MAX_REC_SIZE check could not be done in op_gvincr (like is done in op_gvput) because
			 * the post-increment value is not known until here. so do the check here.
			 */
			ENSURE_VALUE_WITHIN_MAX_REC_SIZE(value);
		} else
			value = val->str;
	}
	/* --------------------------------------------------------------------------------------------
	 * The code for the non-block-split case is very similar to the code in recompute_upd_array.
	 * Any changes in either place should be reflected in the other.
	 * --------------------------------------------------------------------------------------------
	 */
	bh = &gv_target->hist.h[0];
	for ( ; !succeeded; no_pointers = level_0 = FALSE)
	{
		cur_blk_size = ((blk_hdr_ptr_t)bh->buffaddr)->bsiz;
		target_key_size = temp_key->end + 1;
		/* Potential size of a block containing just the new/updated record */
		new_blk_size_single = sizeof(blk_hdr) + sizeof(rec_hdr) + target_key_size + value.len;
		if (new_blk_size_single > blk_reserved_size)
		{	/* The record that is newly inserted/updated does not fit by itself in a separate block
			 * if the current reserved-bytes for this database is taken into account. If this is not a
			 * GVT leaf block, this situation is then possible if we are not in the final retry (and hence
			 * dont hold crit on the region) and "temp_key->end" (and in turn "target_key_size") was
			 * computed from a stale copy (due to concurrent updates or buffer reuse) of the global buffer
			 * (effectively a restartable situation). If so, restart. If not issue error.
			 */
			if (no_pointers || (CDB_STAGNATE <= t_tries))
			{
				ISSUE_RSVDBYTE2HIGH_ERROR;
			} else
			{
				status = cdb_sc_mkblk;
				goto retry;
			}
		}
		curr_rec_match = bh->curr_rec.match;
		curr_rec_offset = bh->curr_rec.offset;
		new_rec = (target_key_size != curr_rec_match);
                if (!new_rec && !no_pointers)
                {
                        assert(CDB_STAGNATE > t_tries);
                        status = cdb_sc_lostcr;         /* will a new cdb_sc status be better */
                        goto retry;
                }
		rp = (rec_hdr_ptr_t)(bh->buffaddr + curr_rec_offset);
		if (curr_rec_offset == cur_blk_size)
		{
			if ((FALSE == new_rec) && (0 < dollar_tlevel))
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				goto retry;
			}
			rec_cmpc = 0;
			rec_size = 0;
		} else
		{
			GET_USHORT(rec_size, &rp->rsiz);
			rec_cmpc = rp->cmpc;
			if ((sm_uc_ptr_t)rp + rec_size > (sm_uc_ptr_t)bh->buffaddr + cur_blk_size)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				goto retry;
			}
		}
		prev_rec_match = bh->prev_rec.match;
		if (new_rec)
		{
			new_rec_size = sizeof(rec_hdr) + target_key_size - prev_rec_match + value.len;
			if (cur_blk_size <= (signed int)curr_rec_offset) /* typecast necessary to enforce "signed int" comparison */
				next_rec_shrink = 0;
			else
				next_rec_shrink = curr_rec_match - rec_cmpc;
			delta = new_rec_size - next_rec_shrink;
		} else
		{
			if (rec_cmpc != prev_rec_match)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				goto retry;
			}
			new_rec_size = sizeof(rec_hdr) + (target_key_size - rec_cmpc) + value.len;
			delta = new_rec_size - rec_size;
			if (!delta && gvdupsetnoop && value.len
				&& !memcmp(value.addr, (sm_uc_ptr_t)rp + new_rec_size - value.len, value.len))
			{
				duplicate_set = TRUE;
				succeeded = TRUE;
				break;	/* duplicate SET */
			}
			next_rec_shrink = 0;
		}
		if (0 < dollar_tlevel)
		{
			if ((sizeof(rec_hdr) + target_key_size - prev_rec_match + value.len) != new_rec_size)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				goto retry;
			}
			chain1 = *(off_chain *)&bh->blk_num;
			if ((1 == chain1.flag) && ((int)chain1.cw_index >= sgm_info_ptr->cw_set_depth))
			{
				assert(&FILE_INFO(sgm_info_ptr->gv_cur_region)->s_addrs == cs_addrs);
				assert(FALSE == cs_addrs->now_crit);
				status = cdb_sc_blknumerr;
				goto retry;
			}
		}
		next_rec_shrink1 = next_rec_shrink;
		/* Potential size of the current block including the new/updated record */
		new_blk_size = cur_blk_size + delta;
		assert(new_blk_size >= new_blk_size_single);
		if ((new_blk_size <= blk_fill_size) || (new_blk_size <= new_blk_size_single))
		{	/* Update can be done without overflowing the block's fillfactor OR the record to be updated
			 * is the only record in the new block. Do not split block in either case. This means we might
			 * not honour the desired FillFactor if the only record in a block exceeds the blk_fill_size,
			 * but in this case we are guaranteed the block has room for the current reserved bytes.
			 */
			if (no_pointers)	/* level zero (normal) data block: no deferred pointer chains */
				ins_chain_offset = 0;
			else			/* index or directory level block */
				ins_chain_offset = (sm_uc_ptr_t)rp - bh->buffaddr + new_rec_size - sizeof(block_id);
			BLK_INIT(bs_ptr, bs1);
			if (0 == rc_set_fragment)
			{
				BLK_SEG(bs_ptr, bh->buffaddr + sizeof(blk_hdr), curr_rec_offset - sizeof(blk_hdr));
				BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
				curr_rec_hdr->rsiz = new_rec_size;
				curr_rec_hdr->cmpc = prev_rec_match;
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
				BLK_ADDR(cp1, target_key_size - prev_rec_match, unsigned char);
				memcpy(cp1, temp_key->base + prev_rec_match, target_key_size - prev_rec_match);
				BLK_SEG(bs_ptr, cp1, target_key_size - prev_rec_match);
				if (0 != value.len)
				{
					BLK_ADDR(va, value.len, char);
					memcpy(va, value.addr, value.len);
					BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
				}
				if (!new_rec)
					rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
				n = cur_blk_size - ((sm_uc_ptr_t)rp - bh->buffaddr);
				if (n > 0)
				{
					if (new_rec)
					{
						BLK_ADDR(next_rec_hdr, sizeof(rec_hdr), rec_hdr);
						next_rec_hdr->rsiz = rec_size - next_rec_shrink;
						next_rec_hdr->cmpc = curr_rec_match;
						BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, sizeof(rec_hdr));
						next_rec_shrink += sizeof(rec_hdr);
					}
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp + next_rec_shrink, n - next_rec_shrink);
				}
			} else
			{
				curr_rec_hdr = (rec_hdr_ptr_t)(bh->buffaddr + curr_rec_offset);
				/* First piece is block prior to record + key + data prior to fragment */
				BLK_SEG(bs_ptr,
					bh->buffaddr + sizeof(blk_hdr),
					curr_rec_offset - sizeof(blk_hdr) + sizeof(rec_hdr) + rc_set_fragment
						+ gv_currkey->end + 1 - curr_rec_hdr->cmpc);
				/* Second piece is fragment itself */
				BLK_ADDR(va, value.len, char);
				memcpy(va, value.addr, value.len);
				BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
				/* Third piece is data after fragment + rest of block after record */
				n = cur_blk_size - ((sm_uc_ptr_t)curr_rec_hdr - bh->buffaddr) - sizeof(rec_hdr)
					- (gv_currkey->end + 1 - curr_rec_hdr->cmpc) - rc_set_fragment - value.len;
				if (0 < n)
					BLK_SEG(bs_ptr,
						(sm_uc_ptr_t)curr_rec_hdr + gv_currkey->end + 1 - curr_rec_hdr->cmpc
							+ rc_set_fragment + value.len,
						n);
			}
			if (0 == BLK_FINI(bs_ptr, bs1))
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				goto retry;
			}
			assert(bs1[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
			cse = t_write(bh, (unsigned char *)bs1, ins_chain_offset, ins_chain_index, bh->level, FALSE, FALSE);
			assert(!dollar_tlevel || !cse->high_tlevel);
			bh->ptr = cse;	/* used only in TP */
			if ((0 != ins_chain_offset) && (NULL != cse) && (0 != cse->first_off))
			{	/* formerly tp_offset_chain - inserts a new_entry in the chain */
				assert(NULL != cse->new_buff || horiz_growth && cse->low_tlevel->new_buff &&
								bh->buffaddr == cse->low_tlevel->new_buff);
				assert(0 == cse->next_off);
				assert(ins_chain_offset > (signed)sizeof(blk_hdr));	/* we want signed comparison */
				assert((curr_rec_offset - sizeof(off_chain)) == (ins_chain_offset - new_rec_size));
				offset_sum = cse->first_off;
				curr = bh->buffaddr + offset_sum;
				/* The typecast is needed below to enforce a "signed int" (versus "unsigned int") comparison */
				if (offset_sum >= (signed int)curr_rec_offset)
				{	/* the new record is prior to the first existing chain record, id the new one as first */
					/* first_off-------------v--------------------v
					 * [blk_hdr]...[new rec ( )]...[existing rec ( )]... */
					cse->next_off = cse->first_off - (ins_chain_offset - new_rec_size) - next_rec_shrink1;
					cse->first_off = ins_chain_offset;
				} else
				{
					if (horiz_growth)
					{
						old_cse = cse->low_tlevel;
						assert(old_cse->first_off);
						assert(old_cse && old_cse->done);
						assert(!old_cse->undo_next_off[0] && !old_cse->undo_offset[0]);
					}
					/* find chain records before and after the new one */
					for ( ; ; curr += curr_chain.next_off)
					{	/* try to make offset_sum identify the first chain entry after the new record */
						GET_LONGP(&curr_chain, curr);
						assert(curr_chain.flag == 1);
						if (0 == curr_chain.next_off)
							break;
						offset_sum += curr_chain.next_off;
						/* The typecast is needed below to enforce a "signed int" comparison */
						if (offset_sum >= (signed int)curr_rec_offset)
							break;
					}
					/* store the next_off in old_cse before chainging it in the buffer (for rolling back) */
					if (horiz_growth)
					{
						old_cse->undo_next_off[0] = curr_chain.next_off;
						old_cse->undo_offset[0] = curr - bh->buffaddr;
						assert(old_cse->undo_offset[0]);
					}
					if (0 == curr_chain.next_off)
					{	/* the last chain record precedes the new record: just update it */
						/* 			   ---|---------------v
						 * [blk_hdr]...[existing rec ( )]...[new rec ( )]... */
						curr_chain.next_off = ins_chain_offset - offset_sum;
						GET_LONGP(curr, &curr_chain);
					} else
					{	/* update the chain record before the new one */
						/* 			   ---|---------------v--------------------v
						 * [blk_hdr]...[existing rec ( )]...[new rec ( )]...[existing rec ( )] */
						curr_chain.next_off = ins_chain_offset - (curr - bh->buffaddr);
						GET_LONGP(curr, &curr_chain);
						cse->next_off = offset_sum - (ins_chain_offset - new_rec_size) - next_rec_shrink1;
					}
				}
				assert((ins_chain_offset + (int)cse->next_off) <=
				       (delta + (sm_long_t)cur_blk_size - sizeof(off_chain)));
			}
			succeeded = TRUE;
			if (level_0 && new_rec)
				gv_target->hist.h[0].curr_rec.match = gv_target->clue.end + 1;	/* Update clue */
		} else
		{	/* Block split required */
			gv_target->clue.end = 0;	/* invalidate clue */
			/* Potential size of the left and right blocks, including the new record */
			new_blk_size_l = curr_rec_offset + new_rec_size;
			new_blk_size_r = sizeof(blk_hdr) + sizeof(rec_hdr) + target_key_size + value.len + cur_blk_size
						- curr_rec_offset - (new_rec ? next_rec_shrink : rec_size);
			assert(new_blk_size_single <= blk_reserved_size);
			assert(blk_reserved_size >= blk_fill_size);
			extra_record_orig_size = 0;
			prev_rec_offset = bh->prev_rec.offset;
			assert(new_blk_size_single <= new_blk_size_r);
			/* Decide which side (left or right) the new record goes. Ensure either side has at least one record.
			 * This means we might not honour the desired FillFactor if the only record in a block exceeds the
			 * blk_fill_size, but in this case we are guaranteed the block has room for the current reserved bytes.
			 * The typecast of curr_rec_offset is needed below to enforce a "signed int" comparison.
			 */
			new_rec_goes_to_right = (new_blk_size_r <= blk_fill_size)
				? (((blk_fill_size / 2) < (signed int)curr_rec_offset) || (blk_fill_size < new_blk_size_l))
				: (new_blk_size_r <= new_blk_size_single);
			if (new_rec_goes_to_right)
			{	/* Left side of this block will be split off into a new block.
				 * The new record and the right side of this block will remain in this block.
				 */
				/* prepare new block */
				BLK_INIT(bs_ptr, bs1);
				if (level_0)
				{
					BLK_SEG(bs_ptr, bh->buffaddr + sizeof(blk_hdr), curr_rec_offset - sizeof(blk_hdr));
				} else
				{	/* for index records, the record before the split becomes a new *-key */
					/* Note:  If the block split was caused by our appending the new record
					 * to the end of the block, this code causes the record PRIOR to the
					 * current *-key to become the new *-key.
					 */
					BLK_SEG(bs_ptr, bh->buffaddr + sizeof(blk_hdr), prev_rec_offset - sizeof(blk_hdr));
					BLK_ADDR(new_star_hdr, sizeof(rec_hdr), rec_hdr);
					new_star_hdr->rsiz = BSTAR_REC_SIZE;
					new_star_hdr->cmpc = 0;
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)new_star_hdr, sizeof(rec_hdr));
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp - sizeof(block_id), sizeof(block_id));
				}
				new_blk_bs = bs1;
				if (0 == BLK_FINI(bs_ptr,bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					goto retry;
				}
				/* It is possible that the left block DOES NOT have enough space for reserved bytes if
				 * the pre-split block was previously populated with a very low reserved bytes setting
				 * and if the current reserved bytes setting is much higher than what the chosen split
				 * point would free up. The following assert checks that at least for reserved bytes
				 * less than or equal to 16, the space constraint is unconditionally met.
				 */
				assert((bs1[0].len <= blk_reserved_size) || reserved_bytes > 16);
				/* prepare the existing block */
				BLK_INIT(bs_ptr, bs1);
				ins_chain_offset = no_pointers ? 0 : sizeof(blk_hdr) + sizeof(rec_hdr) + target_key_size;
				left_hand_offset = left_hand_index
						 = 0;
				if (!new_rec)
					rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
				BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
				curr_rec_hdr->rsiz = target_key_size + sizeof(rec_hdr) + value.len;
				curr_rec_hdr->cmpc = 0;
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
				BLK_ADDR(cp1, target_key_size, unsigned char);
				memcpy(cp1, temp_key->base, target_key_size);
				BLK_SEG(bs_ptr, cp1, target_key_size);
				if (0 != value.len)
				{
					BLK_ADDR(va, value.len, char);
					memcpy(va, value.addr, value.len);
					BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
				}
				if (bh->buffaddr + cur_blk_size > (sm_uc_ptr_t)rp)
				{
					BLK_ADDR(next_rec_hdr, sizeof(rec_hdr), rec_hdr);
					GET_USHORT(next_rec_hdr->rsiz, &rp->rsiz);
					next_rec_hdr->rsiz -= next_rec_shrink;
					next_rec_hdr->cmpc = new_rec ? curr_rec_match : rp->cmpc;
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, sizeof(rec_hdr));
					next_rec_shrink += sizeof(rec_hdr);
					BLK_SEG(bs_ptr,
						(sm_uc_ptr_t)rp + next_rec_shrink,
						cur_blk_size - ((sm_uc_ptr_t)rp - bh->buffaddr) - next_rec_shrink);
				}
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					goto retry;
				}
				assert(bs1[0].len <= blk_reserved_size); /* Assert that right block has space for reserved bytes */
				assert(gv_altkey->top == gv_currkey->top);
				assert(gv_altkey->end < gv_altkey->top);
				temp_key = gv_altkey;
				if (cdb_sc_normal != (status = gvcst_expand_key((blk_hdr_ptr_t)bh->buffaddr, prev_rec_offset,
						temp_key)))
					goto retry;
			} else
			{	/* Insert in left hand (new) block */
				/* If there is only one record on the left hand side, try for two */
				copy_extra_record = ((0 == prev_rec_offset)
							&& level_0
							&& new_rec
							&& (sizeof(blk_hdr) < cur_blk_size));
				BLK_INIT(bs_ptr, bs1);
				if (no_pointers)
					left_hand_offset = 0;
				else
				{
					left_hand_offset = curr_rec_offset + sizeof(rec_hdr);
					if (level_0)
						left_hand_offset += target_key_size - prev_rec_match;
					/* else it is a *-key (implies the child pointer follows immediately) so no need to
					 * change left_hand_offset. Note that if copy_extra_record was TRUE, then we need to
					 * update left_hand_offset as the new record to be inserted will no longer be a *-key
					 * (the extra_record that got copied over will instead be the *-key), but since
					 * copy_extra_record is explicitly not done for index blocks (see "&& level_0" in the
					 * check above) we do not need to change left_hand_offset.
					 */
				}
				left_hand_index = ins_chain_index;
				ins_chain_index = ins_chain_offset = 0;
				BLK_SEG(bs_ptr, bh->buffaddr + sizeof(blk_hdr), curr_rec_offset - sizeof(blk_hdr));
				if (level_0)
				{	/* After the initial split, will this block fit into the new left block?
					 * If not, this pass through gvcst_put_blk will make room and we will do
					 * another block split on the next pass.
					 */
					assert((blk_seg_cnt + sizeof(rec_hdr) + target_key_size - prev_rec_match + value.len)
						== new_blk_size_l);
					assert(new_blk_size_single <= new_blk_size_l);
					assert((new_blk_size_single < new_blk_size_l)
						|| ((0 == prev_rec_offset) && (sizeof(blk_hdr) == curr_rec_offset)));
					assert((new_blk_size_single == new_blk_size_l)
						|| ((sizeof(blk_hdr) <= prev_rec_offset) && (sizeof(blk_hdr) < curr_rec_offset)));
					if ((new_blk_size_l > blk_fill_size) && (new_blk_size_l > new_blk_size_single))
					{	/* There is at least one existing record to the left of the split point.
						 * Do the initial split this pass and make an extra split next pass.
						 */
						*extra_block_split_req = TRUE;
					} else
					{
						BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
						curr_rec_hdr->rsiz = new_rec_size;
						curr_rec_hdr->cmpc = prev_rec_match;
						BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
						BLK_ADDR(cp1, target_key_size - prev_rec_match, unsigned char);
						memcpy(cp1, temp_key->base + prev_rec_match, target_key_size - prev_rec_match);
						BLK_SEG(bs_ptr, cp1, target_key_size - prev_rec_match);
						if (0 != value.len)
						{
							BLK_ADDR(va, value.len, char);
							memcpy(va, value.addr, value.len);
							BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
						}
						if (copy_extra_record)
						{
							n = rec_size - curr_rec_match;
			 				/* typecast needed below to enforce a "signed int" comparison */
							if ((n + (signed int)curr_rec_offset + new_rec_size) > blk_fill_size)
								copy_extra_record = FALSE;
							else
							{
								BLK_ADDR(extra_rec_hdr, sizeof(rec_hdr), rec_hdr);
								extra_rec_hdr->rsiz = n;
								extra_rec_hdr->cmpc = curr_rec_match;
								BLK_SEG(bs_ptr, (sm_uc_ptr_t)extra_rec_hdr, sizeof(rec_hdr));
								if (n < (signed)sizeof(rec_hdr)) /* want signed compare */
								{				     /* as 'n' can be negative */
									assert(CDB_STAGNATE > t_tries);
									status = cdb_sc_mkblk;
									goto retry;
								}
								BLK_SEG(bs_ptr,
									bh->buffaddr + sizeof(blk_hdr) + sizeof(rec_hdr)
										+ curr_rec_match,
									n - sizeof(rec_hdr));
								new_blk_size_l += n;
							}
						}
					}
				} else
				{
					BLK_ADDR(new_star_hdr, sizeof(rec_hdr), rec_hdr);
					new_star_hdr->rsiz = BSTAR_REC_SIZE;
					new_star_hdr->cmpc = 0;
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)new_star_hdr, sizeof(rec_hdr));
					BLK_SEG(bs_ptr, (unsigned char *)&zeroes, sizeof(block_id));
				}
				new_blk_bs = bs1;
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					goto retry;
				}
				/* It is possible that the left block DOES NOT have enough space for reserved bytes if
				 * the pre-split block was previously populated with a very low reserved bytes setting
				 * and if the current reserved bytes setting is much higher than what the chosen split
				 * point would free up. The following assert checks that at least for reserved bytes
				 * less than or equal to 16, the space constraint is unconditionally met.
				 */
				assert((bs1[0].len <= blk_reserved_size) || reserved_bytes > 16);
				/* assert that both !new_rec and copy_extra_record can never be TRUE at the same time */
				assert(new_rec || !copy_extra_record);
				if (!new_rec || copy_extra_record)
				{	/* Should guard for empty block??? */
					rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
					rec_cmpc = rp->cmpc;
					temp_short = rec_size;
					GET_USHORT(rec_size, &rp->rsiz);
				}
				if (copy_extra_record)
				{
				        extra_record_orig_size = temp_short;
					assert(gv_altkey->top == gv_currkey->top);
					assert(gv_altkey->end < gv_altkey->top);
					temp_key = gv_altkey;
					if (cdb_sc_normal !=
						(status = gvcst_expand_key((blk_hdr_ptr_t)bh->buffaddr, curr_rec_offset,
							temp_key)))
						goto retry;
				} else if (temp_key != gv_altkey)
				{
					memcpy(gv_altkey, temp_key, sizeof(gv_key) + temp_key->end);
					temp_key = gv_altkey;
				}
				rec_size += rec_cmpc;
				BLK_INIT(bs_ptr, bs1);
				BLK_ADDR(next_rec_hdr, sizeof(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = rec_size;
				next_rec_hdr->cmpc = 0;
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, sizeof(rec_hdr));
				BLK_ADDR(cp1, rec_cmpc, unsigned char);
				memcpy(cp1, temp_key->base, rec_cmpc);
				BLK_SEG(bs_ptr, cp1, rec_cmpc);
				BLK_SEG(bs_ptr,
					(sm_uc_ptr_t)(rp + 1),
					cur_blk_size - ((sm_uc_ptr_t)rp - bh->buffaddr) - sizeof(rec_hdr));
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					goto retry;
				}
				/* It is possible that the right block DOES NOT have enough space for reserved bytes if
				 * the pre-split block was previously populated with a very low reserved bytes setting
				 * and if the current reserved bytes setting is much higher than what the chosen split
				 * point would free up. The following assert checks that at least for reserved bytes
				 * less than or equal to 16, the space constraint is unconditionally met.
				 */
				assert((bs1[0].len <= blk_reserved_size) || reserved_bytes > 16);
			}
			next_blk_index = t_create(bh->blk_num, (uchar_ptr_t)new_blk_bs, left_hand_offset, left_hand_index,
				bh->level);
			if ((FALSE == no_pointers) && (0 < dollar_tlevel))
			{	/* there may be chains */
				curr_chain = *(off_chain *)&bh->blk_num;
				if (curr_chain.flag == 1)
					tp_get_cw(sgm_info_ptr->first_cw_set, curr_chain.cw_index, &cse);
				else
				{
					if (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use,
												(uint4 *)&bh->blk_num)))
						tp_srch_status = tabent->value;
					else
						tp_srch_status = NULL;
					cse = tp_srch_status ? tp_srch_status->ptr : NULL;
				}
				assert(!cse || !cse->high_tlevel);
			        if ((NULL != cse) && (0 != cse->first_off))
				{	/* there is an existing chain: fix to account for the split */
					assert(NULL != cse->new_buff);
					assert(cse->done);
					assert(0 == cse->next_off);
					cse_new = sgm_info_ptr->last_cw_set;
					assert(!cse_new->high_tlevel);
					assert(0 == cse_new->next_off);
					assert(0 == cse_new->first_off);
					assert(cse_new->ins_off == left_hand_offset);
					assert(cse_new->index == left_hand_index);
					assert(cse_new->level == cse->level);
					offset_sum = cse->first_off;
					curr = bh->buffaddr + offset_sum;
					GET_LONGP(&curr_chain, curr);
					assert(curr_chain.flag == 1);
					last_possible_left_offset = curr_rec_offset + extra_record_orig_size - sizeof(off_chain);
					/* some of the following logic used to be in tp_split_chain which was nixed */
					if (offset_sum <= last_possible_left_offset)
					{	/* the split falls within or after the chain; otherwise entire chain stays right */
						assert(curr_rec_offset != (int4)cse->first_off);
						if (left_hand_offset && (curr_rec_offset < (int4)cse->first_off))
							/* we are inserting the new record (with the to-be-filled child block
							 * number) in the left block and the TP block chain of the block to be
							 * split starts AFTER the new record's offset in the current block.
							 * this means the left block (cse_new) will have a block chain starting
							 * with the newly inserted record's block pointer.
							 */
							cse_new->first_off = left_hand_offset;
						else
							cse_new->first_off = cse->first_off;
						if (level_0)	/* if no *-key issue stop after, rather than at, a match */
							last_possible_left_offset += sizeof(off_chain);
						if (offset_sum < last_possible_left_offset)
						{	/* it's not an immediate hit */
							for ( ; ; curr += curr_chain.next_off)
							{	/* follow chain to split */
								GET_LONGP(&curr_chain, curr);
								assert(curr_chain.flag == 1);
								if (0 == curr_chain.next_off)
									break;
								offset_sum += curr_chain.next_off;
								if (offset_sum >= last_possible_left_offset)
									break;
							}	/* end of search chain loop */
						}
						assert(curr >= (bh->buffaddr + cse->first_off));
						if (level_0)	/* restore match point to "normal" */
							last_possible_left_offset -= sizeof(off_chain);
						if ((offset_sum == last_possible_left_offset) && !level_0)
						{	/* if the chain hits the new last record in an index block,
							 * the search stopped just before the split
							 */
							assert(0 == extra_record_orig_size);
							if (left_hand_offset)
							{	/* the new record will become the *-key: terminate the chain */
								/*		      ---|-------------------v
								 * [blk_hdr]...[curr rec( )][new rec (*-key)( )] */
								assert(!ins_chain_offset);
								if (offset_sum != cse->first_off)
								{	/* bring curr up to the match */
									curr += curr_chain.next_off;
									GET_LONGP(&curr_chain, curr);
								}
								prev_chain = curr_chain;
								assert((left_hand_offset - (curr - bh->buffaddr))
									== BSTAR_REC_SIZE);
								prev_chain.next_off = BSTAR_REC_SIZE;
								assert((curr - bh->buffaddr + prev_chain.next_off)
									<= (new_blk_size_l - sizeof(off_chain)));
								if (dollar_tlevel != cse->t_level)
								{
									assert(dollar_tlevel > cse->t_level);
									assert(!cse->undo_next_off[0] && !cse->undo_offset[0]);
									assert(!cse->undo_next_off[1] && !cse->undo_offset[1]);
									cse->undo_next_off[0] = curr_chain.next_off;
									cse->undo_offset[0] = curr - bh->buffaddr;
								}
								GET_LONGP(curr, &prev_chain);
								offset_sum += curr_chain.next_off;
							} else
							{
								undo_index = 0;
								/* the last record turns into the *-key */
								if (offset_sum == cse->first_off)
								{	/* it's all there is */
									/* first_off --------------------v
									 * [blk_hdr]...[curr rec (*-key)( )] */
									assert(prev_rec_offset >= sizeof(blk_hdr));
									cse_new->first_off = prev_rec_offset + sizeof(rec_hdr);
								} else
								{	/* update the next_off of the previous chain record */
									/*		      ---|--------------------v
									 * [blk_hdr]...[prev rec( )][curr rec (*-key)( )] */
									assert((bh->buffaddr + prev_rec_offset) > curr);
									prev_chain = curr_chain;
									assert((offset_sum - prev_chain.next_off) /* check old */
										== (curr - bh->buffaddr)); /* method equivalent */
									prev_chain.next_off = (prev_rec_offset
										+ sizeof(rec_hdr) - (curr - bh->buffaddr));
									assert((curr - bh->buffaddr + prev_chain.next_off)
										<= ((new_blk_size_l < blk_reserved_size
										? new_blk_size_l : blk_reserved_size)
										- sizeof(off_chain)));
									if (dollar_tlevel != cse->t_level)
									{
										assert(dollar_tlevel > cse->t_level);
										assert(!cse->undo_next_off[0]
											&& !cse->undo_offset[0]);
										assert(!cse->undo_next_off[1]
											&& !cse->undo_offset[1]);
										cse->undo_next_off[0] = curr_chain.next_off;
										cse->undo_offset[0] = curr - bh->buffaddr;
										undo_index = 1;
									}
									GET_LONGP(curr, &prev_chain);
									/* bring curr up to the match */
									curr += curr_chain.next_off;
									GET_LONGP(&curr_chain, curr);
								}
								offset_sum += curr_chain.next_off;
								if (dollar_tlevel != cse->t_level)
								{
									assert(dollar_tlevel > cse->t_level);
									assert(!cse->undo_next_off[undo_index] &&
										!cse->undo_offset[undo_index]);
									cse->undo_next_off[undo_index] = curr_chain.next_off;
									cse->undo_offset[undo_index] = curr - bh->buffaddr;
								}
								curr_chain.next_off = 0;
								GET_LONGP(curr, &curr_chain);
							}
						} else
						{	/* found the split and no *-key issue: just terminate before the split */
							if (offset_sum == cse->first_off)
								offset_sum += curr_chain.next_off;	/* put it in the lead */
							old_curr_chain_next_off = curr_chain.next_off;
							if (left_hand_offset)
							{	/* there's a new chain rec in left */
								assert(!ins_chain_offset);
								if ((extra_record_orig_size) && ((extra_record_orig_size
									- sizeof(off_chain) + sizeof(blk_hdr)) == cse->first_off))
								{	/* extra rec has a chain and it's after the new rec */
									/*		  ---|--------------v
									 * [blk_hdr][new rec( )][extra rec ( )] */
									assert((curr - bh->buffaddr) == cse->first_off);
									assert(level_0);	/* otherwise *-key issue */
									cse_new->next_off = extra_rec_hdr->rsiz;
									curr_chain.next_off = 0;
								} else
								{	/* put the new one at the end of the chain */
									/*		      ---|---------------v
									 * [blk_hdr]...[curr rec( )]...[new rec ( )] */
									/* the new rec may or may not be a *-key */
									assert((offset_sum - curr_chain.next_off) /* check old */
										== (curr - bh->buffaddr)); /* method equivalent */
									curr_chain.next_off = left_hand_offset -
												(curr - bh->buffaddr);
								}
							} else
								curr_chain.next_off = 0;
							assert((curr - bh->buffaddr + curr_chain.next_off)
									<= ((new_blk_size_l < blk_reserved_size
									? new_blk_size_l : blk_reserved_size) - sizeof(off_chain)));
							if (dollar_tlevel != cse->t_level)
							{
								assert(dollar_tlevel > cse->t_level);
								assert(!cse->undo_next_off[0] && !cse->undo_offset[0]);
								assert(!cse->undo_next_off[1] && !cse->undo_offset[1]);
								cse->undo_next_off[0] = old_curr_chain_next_off;
								cse->undo_offset[0] = curr - bh->buffaddr;
							}
							GET_LONGP(curr, &curr_chain);
						}	/* end of *-key or not alternatives */
						assert((left_hand_offset + (int)cse_new->next_off) <=
							((new_blk_size_l < blk_reserved_size ? new_blk_size_l : blk_reserved_size)
								- sizeof(off_chain)));
					}	/* end of buffer and cse_new adjustments */
					prev_first_off = cse->first_off;
					if (ins_chain_offset)
					{	/* if there is a new chain rec in the old block, put it first */
						/* first_off---------v
						 * [blk_hdr][new rec( )]... */
						assert(!left_hand_offset);
						assert(0 == extra_record_orig_size);
						assert(ins_chain_offset >= (sizeof(blk_hdr) + sizeof(rec_hdr)));
						cse->first_off = ins_chain_offset;
						if (offset_sum > last_possible_left_offset)
						{	/* there are existing chain records after the split */
							/* first_off---------v--------------------v
							 * [blk_hdr][new rec( )]...[existing rec ( )] */
							prev_next_off = cse->next_off;
							cse->next_off = offset_sum - last_possible_left_offset - next_rec_shrink1;
							assert((int)(cse->next_off + ins_chain_offset) < new_blk_size_r);
						}
					} else if (offset_sum <= last_possible_left_offset)
					{	/* the last chain record went left with the split */
						cse->first_off = 0;
					} else
					{	/* just adjust the anchor for the split */
						/* first_off------------------v
						 * [blk_hdr]...[existing rec ( )] */
						assert(offset_sum >= (int)cse->first_off);
						cse->first_off = offset_sum - last_possible_left_offset + rec_cmpc
								+ sizeof(blk_hdr) - sizeof(off_chain);
						assert(cse->first_off >= (sizeof(blk_hdr) + sizeof(rec_hdr)));
					}
					assert((ins_chain_offset + (int)cse->next_off) <=
						((new_blk_size_r < blk_reserved_size ? new_blk_size_r : blk_reserved_size)
							- sizeof(off_chain)));
				}	/* end of of split processing */
			}	/* end of tp only code */
			if (0 == dollar_tlevel)
				cse = NULL;
			else
			{
				cse_new = sgm_info_ptr->last_cw_set;
				assert(!cse_new->high_tlevel);
				gvcst_blk_build(cse_new, NULL, 0);
				cse_new->done = TRUE;
			}
			assert(temp_key == gv_altkey);
			if (level_0)
			{
				cp1 = temp_key->base;
				cp2 = (unsigned char *)bs1[2].addr;
				for (i = 0;  i < bs1[2].len  &&  *cp2 == *cp1;  ++i)
				{
					++cp2;
					++cp1;
				}
				if (i == bs1[2].len)
				{
					cp2 = (unsigned char *)bs1[3].addr;
					for (i = 0;  i < bs1[3].len  &&  *cp2 == *cp1;  ++i)
					{
						++cp2;
						++cp1;
					}
				}
				n = ((sm_long_t)*cp2 - (sm_long_t)*cp1) / 2;
				if (n < 0)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					goto retry;
				}
				if (0 != n)
				{
					temp_key->end = cp1 - temp_key->base + 2;
					if (temp_key->end < temp_key->top)
					{
						*cp1++ += n;
						*cp1++ = 0;
						*cp1 = 0;
					} else
					{
						temp_key->end = temp_key->prev;
						assert(temp_key->end < temp_key->top);
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_mkblk;
						goto retry;
					}
				}
			}
			bq = bh + 1;
			if (HIST_TERMINATOR != bq->blk_num)
			{	/* Not root;  write blocks and continue */
				if (cdb_sc_normal != (status = gvcst_search_blk(temp_key, bq)))
					goto retry;
				cse = t_write(bh, (unsigned char *)bs1, ins_chain_offset, ins_chain_index, bh->level, TRUE, FALSE);
				assert(!dollar_tlevel || !cse->high_tlevel);
				if (cse)
				{
					assert(dollar_tlevel);
					cse->write_type |= GDS_WRITE_BLOCK_SPLIT;
				}
				bh->ptr = cse;	/* used only in TP */
				value.len = sizeof(block_id);
				value.addr = (char *)&zeroes;
				++bh;
				ins_chain_index = next_blk_index;
			} else
			{	/* Create new root */
				if ((bh->level + 1) == MAX_BT_DEPTH)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_maxlvl;
					goto retry;
				}
				ins_chain_index = t_create(bh->blk_num, (uchar_ptr_t)bs1, ins_chain_offset, ins_chain_index,
					bh->level);
				make_it_null = FALSE;
				if (NULL != cse)
				{     /* adjust block to use the buffer and offsets worked out for the old root */
					assert(cse->done);
				        assert(NULL != cse->new_buff);
					cse_new = sgm_info_ptr->last_cw_set;
					assert(!cse_new->high_tlevel);
					cse_new->blk_target = cse->blk_target;
					cse_new->first_off = cse->first_off;
					cse_new->next_off = cse->next_off;
							/* to be able to incrementally rollback, we need another copy of new_buff,
							 * pointer copying wouldn't suffice
							 */
					cse_new->new_buff = ((new_buff_buddy_list *)get_new_free_element
										(sgm_info_ptr->new_buff_list))->new_buff;
					memcpy(cse_new->new_buff, cse->new_buff, ((blk_hdr_ptr_t)cse->new_buff)->bsiz);
					cse_new->old_block = NULL;
					make_it_null = TRUE;
				}
				/* Build the right child of the new root right now since it is possible that before commit the
				 * root block may have been recycled in the global buffer which wouldn't cause a restart since
				 * it has been built already (see the gvcst_blk_build below). Otherwise, we may be relying
				 * on incorrect data in the root block when we build this right child finally in bg_update.
				 * Note that this needs to be done only in TP since only tp_tend allows for a block with a
				 * cse not to be in the global buffer if a new_buff already exists.
				 */
				if (dollar_tlevel)
				{
					DEBUG_ONLY(tp_get_cw(sgm_info_ptr->first_cw_set, ins_chain_index, &cse_new);)
					assert(cse_new == sgm_info_ptr->last_cw_set);
					cse_new = sgm_info_ptr->last_cw_set;
					assert(FALSE == cse_new->done);
					assert(!cse_new->high_tlevel);
					gvcst_blk_build(cse_new, NULL, 0);
					cse_new->done = TRUE;
				}
				target_key_size = temp_key->end + 1;
				BLK_INIT(bs_ptr, bs1);
				BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
				curr_rec_hdr->rsiz = target_key_size + sizeof(rec_hdr) + sizeof(block_id);
				curr_rec_hdr->cmpc = 0;
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
				BLK_ADDR(cp1, target_key_size, unsigned char);
				memcpy(cp1, temp_key->base, target_key_size);
				BLK_SEG(bs_ptr, cp1, target_key_size);
				BLK_SEG(bs_ptr, (unsigned char *)&zeroes, sizeof(block_id));
				BLK_ADDR(next_rec_hdr, sizeof(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = BSTAR_REC_SIZE;
				next_rec_hdr->cmpc = 0;
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, sizeof(rec_hdr));
				BLK_SEG(bs_ptr, (unsigned char *)&zeroes, sizeof(block_id));
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					goto retry;
				}
				assert(bs1[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
				ins_off1 = sizeof(blk_hdr) + sizeof(rec_hdr) + target_key_size;
				ins_off2 = sizeof(blk_hdr) + 2 * sizeof(rec_hdr) + sizeof(block_id) + target_key_size;
				assert(ins_off1 < ins_off2);
				cse = t_write(bh, (unsigned char *)bs1, ins_off1, next_blk_index, bh->level + 1, TRUE, FALSE);
				if (make_it_null)
					cse->new_buff = NULL;
				assert(!dollar_tlevel || !cse->high_tlevel);
				if (0 == dollar_tlevel)
				{	/* create a sibling cw-set-element to store ins_off2/ins_chain_index */
					t_write_root(ins_off2, ins_chain_index);
				} else
				{
					bh->ptr = cse;
					cse->write_type |= GDS_WRITE_BLOCK_SPLIT;
					assert(NULL == cse->new_buff);
					cse->first_off = 0;
					cse->next_off = ins_off2 - ins_off1;
					/* the following is the only place where the buffer is not completely built by
					 * gvcst_blk_build. this means that the block chain seen by gvcst_blk_build will
					 * have a bad value (that is fixed below) at the end of the list. therefore the
					 * block chain integrity checking code in gvcst_blk_build will error out normally
					 * in this case. signal that routine to skip checking just this tail element.
					 */
					DEBUG_ONLY(skip_block_chain_tail_check = TRUE;)
					gvcst_blk_build(cse, NULL, 0);
					DEBUG_ONLY(skip_block_chain_tail_check = FALSE;)
					curr_chain.flag = 1;
					curr_chain.cw_index = ins_chain_index;
					curr_chain.next_off = 0;
					curr = cse->new_buff + ins_off2;
					GET_LONGP(curr, &curr_chain);
					cse->done = TRUE;
					gv_target->clue.end = 0;
				}
				succeeded = TRUE;
			}
		}
		/* -----------------------------------------------------------------------------------------------------
		 * We have to maintain information for future recomputation if and only if the following are satisfied
		 *	1) The block is a leaf-level block
		 *	2) We are in TP (indicated by non-null cse)
		 *	3) The global has NOISOLATION turned ON
		 *	4) The cw_set_element hasn't encountered a block-split or a kill
		 *	5) We don't need an extra_block_split
		 *
		 * we can also add an optimization that only cse's of mode gds_t_write need to have such updations, but
		 * 	because of the belief that for a nonisolated variable, we will very rarely encounter a
		 *	situation where a created block (in TP) will have some new keys added to it, and that adding
		 *	the check slows down the normal code, we don't do that check here.
		 * -----------------------------------------------------------------------------------------------------
		 */
		if (level_0 && cse && gv_target->noisolation && !cse->write_type && !*extra_block_split_req)
		{
			assert(dollar_tlevel);
			if (is_dollar_incr)
				rts_error(VARLSTCNT(4) ERR_GVINCRISOLATION, 2,
					gv_target->gvname.var_name.len, gv_target->gvname.var_name.addr);
			if (NULL == cse->recompute_list_tail ||
				0 != memcmp(gv_currkey->base, cse->recompute_list_tail->key.base,
					gv_currkey->top))
			{
				tempkv = (key_cum_value *)get_new_element(sgm_info_ptr->recompute_list, 1);
				tempkv->key = *gv_currkey;
				tempkv->next = NULL;
				memcpy(tempkv->key.base, gv_currkey->base, gv_currkey->end + 1);
				if (NULL == cse->recompute_list_head)
				{
					assert(NULL == cse->recompute_list_tail);
					cse->recompute_list_head = tempkv;
				} else
					cse->recompute_list_tail->next = tempkv;
				cse->recompute_list_tail = tempkv;
			} else
				tempkv = cse->recompute_list_tail;
			assert(0 == val->str.len
				|| (val->str.len == bs1[4].len && 0 == memcmp(val->str.addr, bs1[4].addr, val->str.len)));
			tempkv->value.len = val->str.len;		/* bs1[4].addr is undefined if val->str.len is 0 */
			tempkv->value.addr = (char *)bs1[4].addr;	/* 	but not used in that case, so ok */
		}
	}
	assert(succeeded);
	/* format the journal records only once for non-TP (irrespective of number of restarts).
	 * the only exception is if we are in $INCREMENT in which case we need to reformat since the
	 *	current value (and hence the post-increment value) of the key might be different in different tries.
	 * for TP, restart code causes a change in flow and calls gvcst_put() again which will force us to redo the jnl_format()
	 */
	assert(dollar_tlevel || !jnl_format_done || (JNL_SET == non_tp_jfb_ptr->ja.operation));
	assert(!dollar_tlevel || !jnl_format_done
		|| (JNL_SET == ((jnl_format_buffer *)((uchar_ptr_t)sgm_info_ptr->jnl_tail
								- offsetof(jnl_format_buffer, next)))->ja.operation));
	if (JNL_ENABLED(cs_addrs) && (!jnl_format_done || is_dollar_incr))
	{
		if (0 == dollar_tlevel)
		{
			jfb = non_tp_jfb_ptr; /* already malloced in gvcst_init() */
			jgbl.cumul_jnl_rec_len = 0;
			DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
		} else
		{
			jfb = (jnl_format_buffer *)get_new_element(sgm_info_ptr->jnl_list, 1);
			jfb->next = NULL;
			assert(NULL == *sgm_info_ptr->jnl_tail);
			*sgm_info_ptr->jnl_tail = jfb;
			sgm_info_ptr->jnl_tail = &jfb->next;
		}
		ja = &(jfb->ja);
		ja->key = gv_currkey;
		if (!is_dollar_incr)
			ja->val = val;
		else
			ja->val = post_incr_mval;
		ja->operation = JNL_SET;
		jnl_format(jfb);
		jgbl.cumul_jnl_rec_len += jfb->record_size;
		assert(0 == jgbl.cumul_jnl_rec_len % JNL_REC_START_BNDRY);
		DEBUG_ONLY(jgbl.cumul_index++;)
		jnl_format_done = TRUE;
	}
validate:
	horiz_growth = FALSE;
	assert(cs_addrs->dir_tree == gv_target || tp_root);
	RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
	if (0 == dollar_tlevel)
	{
		if (*extra_block_split_req)
                        inctn_opcode = inctn_gvcstput_extra_blk_split;
                succeeded = ((trans_num)0 != t_end(&gv_target->hist, dir_hist));
                inctn_opcode = inctn_invalid_op;
		if (succeeded)
		{
			++cs_data->n_puts;
			if (duplicate_set)
				++cs_data->n_puts_duplicate;
			if (NULL != dir_hist)
				gv_target->clue.end = 0;	/* Invalidate clue */
		}
	} else
	{
		status = tp_hist(dir_hist);
		if (cdb_sc_normal != status)
			goto retry;
	}
	return succeeded;
retry:
	RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
	t_retry(status);
	assert(0 == dollar_tlevel);
	return FALSE;
}
