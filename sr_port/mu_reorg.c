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

/***************************************************************************************************************
 mu_reorg.c:
	This program reorgs the database block structure of a particular global variable
	traversing the Global Variable Tree (GVT) in a pre-order manner.
	Globals are specified in SELECT option.  During the reorg it does not affect database
	block structure of globals mentioned in EXCLUDE option.
	Given fill_factor (data density % in a block) of a working block, reorg tries to acheive that fill_factor.
	Then it swaps the working block with another block.
	An off-line reorg will assign block-id sequentially following the pre-order traversal.
	An on-line reorg will assign block-id sequentially while traversing the GVT in an adaptive pre-order traversal.
	mu_reorg() calls mu_split(), if split is needed to achieve fill factor
	mu_reorg() calls mu_clsce(), if coalese is needed with right sibling to achieve fill factor
	mu_reorg() calls mu_swap(), to swap the working block which acheived the fill facotr with
 	some other block which will give better I/O performance.
	Note that split can result in increase of GVT height. Coalesce can help to reduce heigth.
	mu_reduce_level() is called to see if height can be reduced.
****************************************************************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdsblkops.h"
#include "gdskill.h"
#include "gdscc.h"
#include "copy.h"
#include "interlock.h"
#include "muextr.h"
#include "mu_reorg.h"
#include "anticipatory_freeze.h"
#include "min_max.h"

/* Include prototypes */
#include "is_proc_alive.h"
#include "t_end.h"
#include "t_retry.h"
#include "mupip_reorg.h"
#include "util.h"
#include "t_begin.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search prototype */
#include "gvcst_bmp_mark_free.h"
#include "gvcst_kill_sort.h"
#include "gtmmsg.h"
#include "add_inter.h"
#include "t_abort.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "memcoherency.h"
#include "change_reg.h"

#ifdef UNIX
#include "repl_msg.h"
#include "gtmsource.h"
#endif

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gv_key			*gv_currkey_next_reorg;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_namehead		*reorg_gv_target;
GBLREF	unsigned char		cw_map_depth;
GBLREF	unsigned char		cw_set_depth;
GBLREF	cw_set_element		cw_set[];
GBLREF	uint4			t_err;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		rdfail_detail;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	kill_set		*kill_set_tail;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	boolean_t		need_kip_incr;
GBLREF	uint4			update_trans;
GBLREF	boolean_t		mu_reorg_in_swap_blk;

error_def(ERR_DBRDONLY);
error_def(ERR_GBLNOEXIST);
error_def(ERR_MAXBTLEVEL);
error_def(ERR_MUREORGFAIL);

#define SAVE_REORG_RESTART													\
{																\
	cs_data->reorg_restart_block = dest_blk_id;										\
	if (OLD_MAX_KEY_SZ >= gv_currkey->end)											\
		memcpy(&cs_data->reorg_restart_key[0], &gv_currkey->base[0], gv_currkey->end + 1);				\
	else															\
	{	/* Save only so much of gv_currkey as will fit in reorg_restart_key. Expect this to be no more than a very	\
		 * minor inconvenience for those using -RESUME */								\
		memcpy(&cs_data->reorg_restart_key[0], &gv_currkey->base[0], OLD_MAX_KEY_SZ + 1);				\
		cs_data->reorg_restart_key[OLD_MAX_KEY_SZ] = 0;									\
		cs_data->reorg_restart_key[OLD_MAX_KEY_SZ - 1] = 0;								\
	}															\
}

#ifdef UNIX
# define ABORT_TRANS_IF_GBL_EXIST_NOMORE_AND_RETURN(LCL_T_TRIES, GN)								\
{																\
	boolean_t		tn_aborted;											\
																\
	ABORT_TRANS_IF_GBL_EXIST_NOMORE(LCL_T_TRIES, tn_aborted);								\
	if (tn_aborted)														\
	{															\
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_GBLNOEXIST, 2, GN->len, GN->addr);				\
		reorg_finish(dest_blk_id, blks_processed, blks_killed, blks_reused, file_extended, lvls_reduced, blks_coalesced,\
				blks_split, blks_swapped);									\
		return TRUE; /* It is not an error if the global (that once existed) doesn't exist anymore (due to ROLLBACK) */	\
	}															\
}
#endif

void log_detailed_log(char *X, srch_hist *Y, srch_hist *Z, int level, kill_set *kill_set_list, trans_num tn);
void reorg_finish(block_id dest_blk_id, block_id blks_processed, block_id blks_killed,
		block_id blks_reused, block_id file_extended, block_id lvls_reduced,
		block_id blks_coalesced, block_id blks_split, block_id blks_swapped);

void log_detailed_log(char *X, srch_hist *Y, srch_hist *Z, int level, kill_set *kill_set_list, trans_num tn)
{
	int		i;
	block_id	bitmap = 1, temp_bitmap;	/* bitmap is initialized to 1, which is not a bitmap block id */

	assert(NULL != (char *)(Y));
	assert(0 < (Y)->depth);
	assert((NULL == (char *)(Z)) || (0 < (Z)->depth));
	util_out_print("!AD::!16@XQ::", FALSE, LEN_AND_STR(X), &tn);
	for (i = 0; i <= (Y)->depth; i++)
		util_out_print("0x!XL|", FALSE, (Y)->h[i].blk_num);
	if (NULL != (char *)(Z))
	{
		util_out_print("-", FALSE);
		for (i = 0; i <= (Z)->depth; i++)
			util_out_print("0x!XL|", FALSE, (Z)->h[i].blk_num);
	}
	if (cw_set_depth)
	{
		util_out_print("::", FALSE);
		for (i = 0; i < cw_set_depth; i++)
			util_out_print("0x!XL|", FALSE, cw_set[i].blk);
	}
	if ((0 == memcmp((X), "SPL", 3))
		|| (0 == memcmp((X), "CLS", 3))
		|| (0 == memcmp((X), "SWA", 3)))
	{
		if (NULL != (char *)(Z))
			util_out_print("::0x!XL|0x!XL", TRUE,
				(Y)->h[level].blk_num, (Z)->h[level].blk_num);
		else
			util_out_print("::0x!XL", TRUE, (Y)->h[level].blk_num);
	} else
	{
		if ((0 == memcmp((X), "KIL", 3)) && (NULL != kill_set_list))
		{
			util_out_print("::", FALSE);
			for (i = 0; i < kill_set_list->used; i++)
			{
				temp_bitmap = kill_set_list->blk[i].block & (~(BLKS_PER_LMAP - 1));
				if (bitmap != temp_bitmap)
				{
					if (1 != bitmap)
						util_out_print("]", FALSE);
					bitmap = temp_bitmap;
					util_out_print("[0x!XL:", FALSE, bitmap);
				}
				util_out_print("0x!XL,", FALSE, kill_set_list->blk[i].block);
			}
			util_out_print("]", FALSE);
		}
		util_out_print(NULL, TRUE);
	}
}

/****************************************************************
Input Parameter:
	gl_ptr = pointer to glist structure corresponding to global name
	exclude_glist_ptr = list of globals in EXCLUDE option
	index_fill_factor = index blocks' fill factor
	data_fill_factor = data blocks' fill factor
Input/Output Parameter:
	resume = resume flag
	reorg_op = What operations to do (coalesce or, swap or, split) [Default is all]
			[Only for debugging]
 ****************************************************************/
boolean_t mu_reorg(glist *gl_ptr, glist *exclude_glist_ptr, boolean_t *resume,
				int index_fill_factor, int data_fill_factor, int reorg_op, const int min_level)
{
	boolean_t		end_of_tree = FALSE, detailed_log;
	int			rec_size, pending_levels;
	/*
	 *
	 * "level" is the level of the working block.
	 * "pre_order_successor_level" is pre_order successor level except in the case
	 * where we are in a left-most descent of the tree
	 * in which case pre_order_successor_level will be the maximum height of that subtree
	 * until we reach the leaf level block .
	 * In other words, pre_order_successor_level and level variable controls the iterative pre-order traversal.
	 * We start reorg from the root_level to min level. That is, level = pre_order_successor_level:-1:min_level.
	 */
	int			pre_order_successor_level, level, merge_split_level;
	static block_id		dest_blk_id = 0;
	int			tkeysize, altkeylen, tkeylen, tkeycmpc;
	block_id		blks_killed, blks_processed, blks_reused, blks_coalesced, blks_split, blks_swapped,
				file_extended, lvls_reduced;
	int			d_max_fill, i_max_fill, blk_size, cur_blk_size, max_fill, toler, d_clsce_toler, i_clsce_toler,
				i_rsrvbytes_maxsz, d_rsrvbytes_maxsz, d_split_toler, i_split_toler, i_rsrv_bytes, d_rsrv_bytes;
	int			cnt1, cnt2, max_rightblk_lvl, count, rtsib_bstar_rec_sz = -1;
	unsigned short		temp_ushort;
	boolean_t		long_blk_id, rtsib_long_blk_id;
	kill_set		kill_set_list;
	sm_uc_ptr_t		rPtr1;
	enum cdb_sc		status;
	srch_hist		*rtsib_hist;
	super_srch_hist		super_dest_hist; /* dir_hist combined with reorg_gv_target->hist */
	jnl_buffer_ptr_t	jbp;
	trans_num		ret_tn;
	mstr			*gn;
	uint4			sleep_nsec;
#	ifdef UNIX
	DEBUG_ONLY(unsigned int	lcl_t_tries;)
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	t_err = ERR_MUREORGFAIL;
	kill_set_tail = &kill_set_list;
	inctn_opcode = inctn_invalid_op; /* temporary reset; satisfy an assert in t_end() */
	DO_OP_GVNAME(gl_ptr);
		/* sets gv_target/gv_currkey/gv_cur_region/cs_addrs/cs_data to correspond to <globalname,reg> in gl_ptr */
	/* Cannot proceed for read-only data files */
	if (gv_cur_region->read_only)
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		return FALSE;
	}
	if (0 == gv_target->root)
		return TRUE; /* It is not an error that global was killed */
	dest_blk_id = cs_addrs->reorg_last_dest;
	inctn_opcode = inctn_mu_reorg;
	gn = &GNAME(gl_ptr);
	/* If resume option is present, then reorg_restart_key should be not null.
	 * Skip all globals until we are in the region for that global.
	 * Get the reorg_restart_key and reorg_restart_block from database header and restart from there.
	 */
	if (*resume && 0 != cs_data->reorg_restart_key[0])
	{	/* resume from last key reorged in GVT */
		tkeysize = get_key_len(NULL, &cs_data->reorg_restart_key[0]);
		memcpy(gv_currkey->base, cs_data->reorg_restart_key, tkeysize);
		gv_currkey->end = tkeysize - 1;
		dest_blk_id = cs_data->reorg_restart_block;
		SET_GV_ALTKEY_TO_GBLNAME_FROM_GV_CURRKEY;
		altkeylen = gv_altkey->end - 1;
		if (altkeylen && (altkeylen == gn->len) && (0 == memcmp(gv_altkey->base, gn->addr, gn->len)))
			/* Going to resume from current global, so it resumed and make it false */
			*resume = FALSE;
	} else
	{	/* start from the left most leaf */
		memcpy(&gv_currkey->base[0], gn->addr, gn->len);
		gv_currkey->base[gn->len] = gv_currkey->base[gn->len + 1] = 0;
		gv_currkey->end = gn->len + 1;
	}
	if (*resume)
	{
		util_out_print("REORG cannot be resumed from this point, Skipping this global...", FLUSH);
		memcpy(&gv_currkey->base[0], gn->addr, gn->len);
		gv_currkey->base[gn->len] = gv_currkey->base[gn->len + 1] = 0;
		gv_currkey->end = gn->len + 1;
		return TRUE;
	}
	long_blk_id = (BLK_ID_32_VER < cs_data->desired_db_format);
	if ((0 != cs_addrs->nl->reorg_upgrade_pid) && (is_proc_alive(cs_addrs->nl->reorg_upgrade_pid, 0)))
		return FALSE;	/* REORG -UPGRADE cannot run concurrently with TRUNCATE which has higher priority. Stop */
	memcpy(&gv_currkey_next_reorg->base[0], &gv_currkey->base[0], gv_currkey->end + 1);
	gv_currkey_next_reorg->end =  gv_currkey->end;
	if (2 > dest_blk_id)
		dest_blk_id = 2; /* we know that first block is bitmap and next one is directory tree root */
	file_extended = cs_data->trans_hist.total_blks;
	blk_size = cs_data->blk_size;
<<<<<<< HEAD
	d_max_fill = (double)data_fill_factor * (blk_size - cs_data->reserved_bytes) / 100;
	i_max_fill = (double)index_fill_factor * (blk_size - cs_data->reserved_bytes) / 100;
	d_toler = (double) DATA_FILL_TOLERANCE * blk_size / 100.0;
	i_toler = (double) INDEX_FILL_TOLERANCE * blk_size / 100.0;
=======
	/* Enforce a minimum *_max_fill. It would be simplest to make this the same as what we take as the smallest acceptable
	 * FILL_FACTOR in mupip_reorg (30% of the block size), but there are cases where the smallest safe effective block size
	 * given by (blk_size - MAX_RESERVE_B) is larger than that. So take into account the maximum reserved bytes and enforce
	 * a minimum *_max_fill of whatever would be the minimum fill if just considering reserved bytes.
	 */
	d_rsrv_bytes = cs_data->reserved_bytes;
	d_max_fill = (double)data_fill_factor * blk_size / 100.0 - d_rsrv_bytes;
	if (d_max_fill <= ((blk_size - MAX_RESERVE_B(cs_data, long_blk_id))))
		d_max_fill = (blk_size - MAX_RESERVE_B(cs_data, long_blk_id));
	d_rsrvbytes_maxsz = blk_size - d_rsrv_bytes;

	i_rsrv_bytes = cs_data->i_reserved_bytes;
	i_max_fill = (double)index_fill_factor * blk_size / 100.0 - i_rsrv_bytes;
	if (i_max_fill <= ((blk_size - MAX_RESERVE_B(cs_data, long_blk_id))))
		i_max_fill = (blk_size - MAX_RESERVE_B(cs_data, long_blk_id));
	i_rsrvbytes_maxsz = blk_size - i_rsrv_bytes;
	assert(d_rsrvbytes_maxsz >= d_max_fill);
	assert(i_rsrvbytes_maxsz >= i_max_fill);
	d_split_toler = d_clsce_toler = (double) DATA_FILL_TOLERANCE * blk_size / 100.0;
	/* Make sure we always try to split if the block exceeds reserved bytes */
	if ((d_split_toler + d_max_fill) > d_rsrvbytes_maxsz)
		d_split_toler = d_rsrvbytes_maxsz - d_max_fill;
	i_split_toler = i_clsce_toler = (double) INDEX_FILL_TOLERANCE * blk_size / 100.0;
	if ((i_split_toler + i_max_fill) > i_rsrvbytes_maxsz)
		i_split_toler = i_rsrvbytes_maxsz - i_max_fill;

>>>>>>> fdfdea1e (GT.M V7.1-002)
	blks_killed = blks_processed = blks_reused = lvls_reduced = blks_coalesced = blks_split = blks_swapped = 0;
	level = pre_order_successor_level = MAX_BT_DEPTH + 1; /* Just some high value to initialize */

	/* --- more detailed debugging information --- */
	if ((detailed_log = reorg_op & DETAIL))
		util_out_print("STARTING to work on global ^!AD from region !AD", TRUE,
			gn->len, gn->addr, REG_LEN_STR(gv_cur_region));

	/* In each iteration of MAIN loop, a working block is processed for a GVT */
	for (; ;)	/* ================ START MAIN LOOP ================ */
	{
		/* If right sibling is completely merged with the working block, do not swap the working block
		 * with its final destination block. Continue trying next right sibling. Swap only at the end.
		 */
		/* We always start with a single pending level for the split-coalesce loop. More levels can be
		 * enqueued by coalesces, which force us to revisit parents for split-coalesce reprocessing
		 */
		pending_levels = 1;
		merge_split_level = level;
		while(pending_levels)	/* === START WHILE COMPLETE_MERGE === */
		{
			assert(pending_levels >= 0);
			if (mu_ctrlc_occurred || mu_ctrly_occurred || ((0 != cs_addrs->nl->reorg_upgrade_pid)
						&& (is_proc_alive(cs_addrs->nl->reorg_upgrade_pid, 0))))
			{	/* REORG -UPGRADE cannot run concurrently with REORG which has higher priority. Stop */
				SAVE_REORG_RESTART;
				return FALSE;
			}
			pending_levels--;
			if (pending_levels)
				merge_split_level++;
			else
				merge_split_level = level;
			blks_processed++;
			t_begin(ERR_MUREORGFAIL, UPDTRNS_DB_UPDATED_MASK);
			/* Following for loop is to handle concurrency retry for split/coalesce */
			for (; ;)		/* === SPLIT-COALESCE LOOP STARTS === */
			{
				gv_target->clue.end = 0;
				/* search gv_currkey and get the result in gv_target */
				if ((status = gvcst_search(gv_currkey, NULL)) != cdb_sc_normal)
				{
					t_retry(status);
					continue;
				}
				if (gv_target->hist.depth < level)
				{
					/* Will come here
					 * 	1) first iteration of the for loop (since level == MAX_BT_DEPTH + 1) or,
					 *	2) tree depth decreased for mu_reduce_level or, M-kill
					 */
					if (min_level > gv_target->hist.depth)
					{
						util_out_print("REORG may be incomplete for this global.", TRUE);
						reorg_finish(dest_blk_id, blks_processed, blks_killed, blks_reused,
							file_extended, lvls_reduced, blks_coalesced, blks_split, blks_swapped);
						return TRUE;

					}
					pre_order_successor_level = min_level;
					/* break the loop when tree depth decreased (case 2) */
					if (MAX_BT_DEPTH + 1 != level)
					{
						level = merge_split_level = gv_target->hist.depth;
						break;
					}
					level = merge_split_level = gv_target->hist.depth;
				}
<<<<<<< HEAD
				max_fill = (0 == level) ? d_max_fill : i_max_fill;
				assert(0 <= max_fill);
				toler = (0 == level) ? d_toler : i_toler;
				cur_blk_size =  ((blk_hdr_ptr_t)(gv_target->hist.h[level].buffaddr))->bsiz;
				if ((cur_blk_size > (max_fill + toler)) && (0 == (reorg_op & NOSPLIT))) /* SPLIT BLOCK */
=======
				assert(merge_split_level >= min_level);
				max_fill = (0 == merge_split_level) ? d_max_fill : i_max_fill;
				toler = (0 == merge_split_level) ? d_split_toler : i_split_toler;
				cur_blk_size =  ((blk_hdr_ptr_t)(gv_target->hist.h[merge_split_level].buffaddr))->bsiz;
				if (!pending_levels			/* While levels need reprocessing, avoid block split */
						&& (cur_blk_size > (max_fill + toler)) /* Check for block split */
						&& (0 == (reorg_op & NOSPLIT))) /* SPLIT BLOCK is ON*/
>>>>>>> fdfdea1e (GT.M V7.1-002)
				{

					assert(merge_split_level == level);
					cnt1 = cnt2 = 0;
					max_rightblk_lvl = merge_split_level;
					/* history of current working block is in gv_target */
					status = mu_split(merge_split_level, i_max_fill, d_max_fill, &cnt1, &cnt2,
							&max_rightblk_lvl);
					if (cdb_sc_maxlvl == status)
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_MAXBTLEVEL, 4, gn->len, gn->addr,
							REG_LEN_STR(gv_cur_region));
						reorg_finish(dest_blk_id, blks_processed, blks_killed, blks_reused,
							file_extended, lvls_reduced, blks_coalesced, blks_split, blks_swapped);
						return FALSE;
					} else if (cdb_sc_normal == status)
					{
						UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
						if ((trans_num)0 == (ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED)))
						{
							need_kip_incr = FALSE;
							assert(NULL == kip_csa);
							UNIX_ONLY(ABORT_TRANS_IF_GBL_EXIST_NOMORE_AND_RETURN(lcl_t_tries, gn));
							max_rightblk_lvl = min_level;
							continue;
						}
						if (detailed_log)
							log_detailed_log("SPL", &(gv_target->hist), NULL, merge_split_level,
									NULL, ret_tn);
						blks_reused += cnt1;
						lvls_reduced -= cnt2;
						blks_split++;
						/* If mu_split created any new righthand index blocks which include the current
						 * record's ancestor, then we need to ensure we fully traverse these blocks. This
						 * means traversing them from the top-down, since it's possible to split off
						 * several righthand blocks from multiple levels of the tree in one mu_split.
						 */
						if (max_rightblk_lvl > merge_split_level)
							pre_order_successor_level = MAX(max_rightblk_lvl,
									pre_order_successor_level);
						max_rightblk_lvl = min_level;
						break;
					} else if (cdb_sc_oprnotneeded == status)
					{	/* undo any update_array/cw_set changes and DROP THRU to mu_clsce */
						cw_set_depth = 0;
						CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
						assert(0 == cw_map_depth); /* mu_swap_blk (that changes cw_map_depth) comes later */
					} else
					{
						t_retry(status);
						continue;
					}
				} /* end if SPLIT BLOCK */
				/* We are here because, mu_split() was not called or, split was not done or, not required */
				rtsib_hist = gv_target->alt_hist;
				status = gvcst_rtsib(rtsib_hist, merge_split_level);
				if (cdb_sc_normal != status && cdb_sc_endtree != status)
				{
					t_retry(status);
					continue;
				}
				if (cdb_sc_endtree == status)
				{
					if (min_level == merge_split_level)
						end_of_tree = TRUE;
					break;
				} else if (min_level == merge_split_level)
					pre_order_successor_level = MAX((rtsib_hist->depth - 1), pre_order_successor_level);
				/* COALESCE WITH RTSIB */
				kill_set_list.used = 0;
				toler = (0 == merge_split_level) ? d_clsce_toler : i_clsce_toler;
				if (cur_blk_size < max_fill - toler && 0 == (reorg_op & NOCOALESCE))
				{
					/* histories are sent in &gv_target->hist and gv_target->alt_hist */

					status = mu_clsce(merge_split_level, i_max_fill, d_max_fill, &kill_set_list,
							&pending_levels);
					if (cdb_sc_normal == status)
					{
						if (merge_split_level) /* delete lower elements of array, t_end might confuse */
						{
							memmove(&rtsib_hist->h[0], &rtsib_hist->h[merge_split_level],
									SIZEOF(srch_blk_status)*(rtsib_hist->depth
										- merge_split_level + 2));
							rtsib_hist->depth = rtsib_hist->depth - merge_split_level;
						}
						if (0 < kill_set_list.used)     /* increase kill_in_prog */
						{
							need_kip_incr = TRUE;
							if (!cs_addrs->now_crit)	/* Do not sleep while holding crit */
								WAIT_ON_INHIBIT_KILLS(cs_addrs->nl, MAXWAIT2KILL);
						}
						UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
						if ((trans_num)0 == (ret_tn = t_end(&(gv_target->hist), rtsib_hist,
							TN_NOT_SPECIFIED)))
						{
							need_kip_incr = FALSE;
							assert(NULL == kip_csa);
							UNIX_ONLY(ABORT_TRANS_IF_GBL_EXIST_NOMORE_AND_RETURN(lcl_t_tries, gn));
							if (merge_split_level)
							{	/* reinitialize level member in rtsib_hist srch_blk_status' */
								for (count = 0; count < MAX_BT_DEPTH; count++)
									rtsib_hist->h[count].level = count;
							}
							continue;
						}
						if (merge_split_level)
						{	/* reinitialize level member in rtsib_hist srch_blk_status' */
							for (count = 0; count < MAX_BT_DEPTH; count++)
								rtsib_hist->h[count].level = count;
						}
						if (detailed_log)
							log_detailed_log("CLS", &(gv_target->hist), rtsib_hist, merge_split_level,
								NULL, ret_tn);
						assert(0 < kill_set_list.used || (NULL == kip_csa));
						if (0 < kill_set_list.used)     /* decrease kill_in_prog */
						{
							gvcst_kill_sort(&kill_set_list);
							GVCST_BMP_MARK_FREE(&kill_set_list, ret_tn, inctn_mu_reorg,
									inctn_bmp_mark_free_mu_reorg, inctn_opcode, cs_addrs)
							DECR_KIP(cs_data, cs_addrs, kip_csa);
							DEFERRED_EXIT_REORG_CHECK;
							if (detailed_log)
								log_detailed_log("KIL", &(gv_target->hist), NULL, merge_split_level,
									&kill_set_list, ret_tn);
							blks_killed += kill_set_list.used;
						}
						blks_coalesced++;
						break;
					} else if (cdb_sc_oprnotneeded == status)
					{	/* undo any update_array/cw_set changes and DROP THRU to t_end */
						cw_set_depth = 0;
						CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
						assert(0 == cw_map_depth); /* mu_swap_blk (that changes cw_map_depth) comes later */
					} else
					{
						t_retry(status);
						continue;
					}
				} /* end if try coalesce */
				if (min_level == merge_split_level)
				{
					/* Note: In min level:
					 *      if split is successful or,
					 *	if coalesce is successful without a complete merge of rtsib,
					 *	then gv_currkey_next_reorg is already set from the called function.
					 *	if split or, coalesce do a retry or,
					 *	if coalesce is successful with a complete merge then
					 *	gv_currkey will not be changed.
					 * If split or, coalesce is not successful or, not needed then
					 *	here gv_currkey_next_reorg will be set from right sibling
					 */
					cw_set_depth = cw_map_depth = 0;
					if (merge_split_level)
					{
						GET_RSIZ(rec_size, (rtsib_hist->h[merge_split_level].buffaddr + SIZEOF(blk_hdr)));
						rtsib_long_blk_id = IS_64_BLK_ID(rtsib_hist->h[merge_split_level].buffaddr);
						rtsib_bstar_rec_sz = bstar_rec_size(rtsib_long_blk_id);
						status = gvcst_expand_any_key(&rtsib_hist->h[merge_split_level],
								rtsib_hist->h[merge_split_level].buffaddr + SIZEOF(blk_hdr)
								+ rec_size, gv_currkey_next_reorg->base, &rec_size, &tkeylen,
								&tkeycmpc, rtsib_hist);
						if (cdb_sc_normal != status)
						{
							t_retry(status);
							continue;
						}
						tkeysize = tkeycmpc + tkeylen;
						if (rtsib_bstar_rec_sz != rec_size)
						{
							/* If gvcst_expand_any_key found a star-key and therefore needed
							 * to process all righthand children, do t_qreads on those blocks,
							 * and initialize rtsib_hist->h[min_level - 1] to rtsib_hist->h[0],
							 * then we need to do nothing. Otherwise, those lower levels can have
							 * garbage data left over and confuse t_end, so we need to fix here
							 * and clean up after.
							 */
							memmove(&rtsib_hist->h[0], &rtsib_hist->h[merge_split_level],
									SIZEOF(srch_blk_status)*(rtsib_hist->depth
										- merge_split_level + 2));
							rtsib_hist->depth = rtsib_hist->depth - merge_split_level;

						}
					} else
						tkeysize = get_key_len(rtsib_hist->h[merge_split_level].buffaddr,
							rtsib_hist->h[merge_split_level].buffaddr + SIZEOF(blk_hdr)
							+ SIZEOF(rec_hdr));
					if (2 < tkeysize && MAX_KEY_SZ >= tkeysize)
					{
						if (!merge_split_level)
							memcpy(&(gv_currkey_next_reorg->base[0]), rtsib_hist->h[0].buffaddr
									+ SIZEOF(blk_hdr) +SIZEOF(rec_hdr), tkeysize);
						gv_currkey_next_reorg->end = tkeysize - 1;
						inctn_opcode = inctn_invalid_op; /* temporary reset; satisfy an assert in t_end() */
						assert(UPDTRNS_DB_UPDATED_MASK == update_trans);
						update_trans = 0; /* tell t_end, this is no longer an update transaction */
						UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
						if ((trans_num)0 == (ret_tn = t_end(rtsib_hist, NULL, TN_NOT_SPECIFIED)))
						{
							need_kip_incr = FALSE;
							assert(NULL == kip_csa);
							UNIX_ONLY(ABORT_TRANS_IF_GBL_EXIST_NOMORE_AND_RETURN(lcl_t_tries, gn));
							if (merge_split_level && (rtsib_bstar_rec_sz != rec_size))
							{	/* reinitialize level member in rtsib_hist srch_blk_status' */
								for (count = 0; count < MAX_BT_DEPTH; count++)
									rtsib_hist->h[count].level = count;
							}
							inctn_opcode = inctn_mu_reorg;	/* reset inctn_opcode to its default */
							update_trans = UPDTRNS_DB_UPDATED_MASK;/* reset update_trans to old value */
							continue;
						}
						/* There is no need to reset update_trans in case of a successful "t_end" call.
						 * This is because before the next call to "t_end" we should have a call to
						 * "t_begin" which will reset update_trans anyways.
						 */
						if (merge_split_level && (rtsib_bstar_rec_sz != rec_size))
						{	/* reinitialize level member in rtsib_hist srch_blk_status' */
							for (count = 0; count < MAX_BT_DEPTH; count++)
								rtsib_hist->h[count].level = count;
						}
						inctn_opcode = inctn_mu_reorg;	/* reset inctn_opcode to its default */
						if (detailed_log)
							log_detailed_log("NOU", rtsib_hist, NULL, merge_split_level, NULL, ret_tn);
					} else
					{
						if (merge_split_level && (rtsib_bstar_rec_sz != rec_size))
						{	/* reinitialize level member in rtsib_hist srch_blk_status' */
							for (count = 0; count < MAX_BT_DEPTH; count++)
								rtsib_hist->h[count].level = count;
						}
						t_retry(status);
						continue;
					}
				} /* end if (0 == level) */
				break;
			}/* === SPLIT-COALESCE LOOP END === */
			t_abort(gv_cur_region, cs_addrs);	/* do crit and other cleanup */
		}/* === START WHILE COMPLETE_MERGE === */
		if (mu_ctrlc_occurred || mu_ctrly_occurred || ((0 != cs_addrs->nl->reorg_upgrade_pid)
					&& (is_proc_alive(cs_addrs->nl->reorg_upgrade_pid, 0))))
		{	/* REORG -UPGRADE cannot run concurrently with TRUNCATE which has higher priority. Stop */
			SAVE_REORG_RESTART;
			return FALSE;
		}
		/* Now swap the working block */
		if (0 == (reorg_op & NOSWAP))
		{
			t_begin(ERR_MUREORGFAIL, UPDTRNS_DB_UPDATED_MASK);
			/* Following loop is to handle concurrency retry for swap */
			for (; ;)	/* === START OF SWAP LOOP === */
			{
				kill_set_list.used = 0;
				gv_target->clue.end = 0;
				/* search gv_currkey and get the result in gv_target */
				if ((status = gvcst_search(gv_currkey, NULL)) != cdb_sc_normal)
				{
					t_retry(status);
					continue;
				}
				if (gv_target->hist.depth <= level)
					break;
				/* Swap working block with appropriate dest_blk_id block.
				 * Histories are sent as gv_target->hist and reorg_gv_target->hist.
				 */
				mu_reorg_in_swap_blk = TRUE;
				status = mu_swap_blk(level, &dest_blk_id, &kill_set_list, exclude_glist_ptr, 0); /* not upgrade */
				mu_reorg_in_swap_blk = FALSE;
				if (cdb_sc_oprnotneeded == status)
				{
					if (cs_data->trans_hist.total_blks <= dest_blk_id)
					{
						util_out_print("REORG may be incomplete for this global.", TRUE);
						reorg_finish(dest_blk_id, blks_processed, blks_killed, blks_reused,
							file_extended, lvls_reduced, blks_coalesced, blks_split, blks_swapped);
						return TRUE;
					}
				} else if (cdb_sc_normal == status)
				{
					UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
					MERGE_SUPER_HIST(&super_dest_hist, reorg_gv_target->alt_hist, &(reorg_gv_target->hist));
					if (0 < kill_set_list.used)
					{
						need_kip_incr = TRUE;
						if (!cs_addrs->now_crit)	/* Do not sleep while holding crit */
							WAIT_ON_INHIBIT_KILLS(cs_addrs->nl, MAXWAIT2KILL);
						/* second history not needed, because,
						   we are reusing a free block, which does not need history */
						if ((trans_num)0 == (ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED)))
						{
							need_kip_incr = FALSE;
							assert(NULL == kip_csa);
							UNIX_ONLY(ABORT_TRANS_IF_GBL_EXIST_NOMORE_AND_RETURN(lcl_t_tries, gn));
							DECR_BLK_NUM(dest_blk_id);
							continue;
						}
						if (detailed_log)
							log_detailed_log("SWA", &(gv_target->hist), NULL, level, NULL, ret_tn);
						gvcst_kill_sort(&kill_set_list);
						GVCST_BMP_MARK_FREE(&kill_set_list, ret_tn, inctn_mu_reorg,
								inctn_bmp_mark_free_mu_reorg, inctn_opcode, cs_addrs)
						DECR_KIP(cs_data, cs_addrs, kip_csa);
						DEFERRED_EXIT_REORG_CHECK;
						if (detailed_log)
							log_detailed_log("KIL", &(gv_target->hist), NULL, level,
								&kill_set_list, ret_tn);
						blks_reused += kill_set_list.used;
						blks_killed += kill_set_list.used;
					}
					/* gv_target->hist is for working block's history, and
					   reorg_gv_target->hist is for destinition block's history.
					   Note: gv_target and reorg_gv_target can be part of different GVT.  */
					else if ((trans_num)0 == (ret_tn = t_end(&(gv_target->hist), (srch_hist *)&super_dest_hist,
						TN_NOT_SPECIFIED)))
					{
						need_kip_incr = FALSE;
						assert(NULL == kip_csa);
						UNIX_ONLY(ABORT_TRANS_IF_GBL_EXIST_NOMORE_AND_RETURN(lcl_t_tries, gn));
						DECR_BLK_NUM(dest_blk_id);
						continue;
					}
					if ((0 >= kill_set_list.used) && detailed_log)
						log_detailed_log("SWA", &(gv_target->hist), &(reorg_gv_target->hist),
							level, NULL, ret_tn);
					blks_swapped++;
					if (reorg_op & SWAPHIST)
						util_out_print("Dest 0x!XL From 0x!XL", TRUE, dest_blk_id,
							gv_target->hist.h[level].blk_num);
				} else
				{
					t_retry(status);
					continue;
				}
				break;
			}	/* === END OF SWAP LOOP === */
			t_abort(gv_cur_region, cs_addrs);	/* do crit and other cleanup */
		}
		if (mu_ctrlc_occurred || mu_ctrly_occurred || ((0 != cs_addrs->nl->reorg_upgrade_pid)
					&& (is_proc_alive(cs_addrs->nl->reorg_upgrade_pid, 0))))
		{	/* REORG -UPGRADE cannot run concurrently with TRUNCATE which has higher priority. Stop */
			SAVE_REORG_RESTART;
			return FALSE;
		}
		if (end_of_tree)
			break;
		if (min_level < level)
			level--; /* Order of reorg is root towards leaf */
		else
		{
			level = pre_order_successor_level;
			pre_order_successor_level = min_level;
			assert(gv_currkey_next_reorg->end < gv_currkey->top);
			memcpy(&gv_currkey->base[0], &gv_currkey_next_reorg->base[0], gv_currkey_next_reorg->end + 1);
			gv_currkey->end =  gv_currkey_next_reorg->end;
			SAVE_REORG_RESTART;
		}
		/* Check if we need to be nice to other processes (i.e. not slowing them down if they are restarting due
		 * to reorg's changes to the database file header).
		 */
		sleep_nsec = cs_data->reorg_sleep_nsec;
		assert((0 <= sleep_nsec) && (NANOSECS_IN_SEC > sleep_nsec));
		if (sleep_nsec)
			NANOSLEEP(sleep_nsec, RESTART_TRUE, MT_SAFE_TRUE);
	}	/* ================ END MAIN LOOP ================ */

	/* =========== START REDUCE LEVEL ============== */
	memcpy(&gv_currkey->base[0], gn->addr, gn->len);
	gv_currkey->base[gn->len] = gv_currkey->base[gn->len + 1] = 0;
	gv_currkey->end = gn->len + 1;
	for (;;)	/* Reduce level continues until it fails to reduce */
	{
		t_begin(ERR_MUREORGFAIL, UPDTRNS_DB_UPDATED_MASK);
		cnt1 = 0;
		for (; ;) 	/* main reduce level loop starts */
		{
			kill_set_list.used = 0;
			gv_target->clue.end = 0;
			/* search gv_currkey and get the result in gv_target */
			if ((status = gvcst_search(gv_currkey, NULL)) != cdb_sc_normal)
			{
				t_retry(status);
				continue;
			}
			if (gv_target->hist.depth <= level)
				break;
			/* History is passed in gv_target->hist */
			status = mu_reduce_level(&kill_set_list);
			if (cdb_sc_oprnotneeded != status && cdb_sc_normal != status)
			{
				t_retry(status);
				continue;
			} else if (cdb_sc_normal == status)
			{
				assert(0 < kill_set_list.used);
				need_kip_incr = TRUE;
				if (!cs_addrs->now_crit)	/* Do not sleep while holding crit */
					WAIT_ON_INHIBIT_KILLS(cs_addrs->nl, MAXWAIT2KILL);
				UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
				if ((trans_num)0 == (ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED)))
				{
					need_kip_incr = FALSE;
					assert(NULL == kip_csa);
					UNIX_ONLY(ABORT_TRANS_IF_GBL_EXIST_NOMORE_AND_RETURN(lcl_t_tries, gn));
					continue;
				}
				if (detailed_log)
					log_detailed_log("RDL", &(gv_target->hist), NULL, level, NULL, ret_tn);
				gvcst_kill_sort(&kill_set_list);
				GVCST_BMP_MARK_FREE(&kill_set_list, ret_tn, inctn_mu_reorg,
						inctn_bmp_mark_free_mu_reorg, inctn_opcode, cs_addrs)
				DECR_KIP(cs_data, cs_addrs, kip_csa);
				DEFERRED_EXIT_REORG_CHECK;
				if (detailed_log)
					log_detailed_log("KIL", &(gv_target->hist), NULL, level, &kill_set_list, ret_tn);
				blks_reused += kill_set_list.used;
				blks_killed += kill_set_list.used;
				cnt1 = 1;
				lvls_reduced++;
			}
			break;
		}		/* main reduce level loop ends */
		t_abort(gv_cur_region, cs_addrs); /* do crit and other cleanup */
		if (0 == cnt1)
			break;
	}
	/* =========== END REDUCE LEVEL ===========*/
	reorg_finish(dest_blk_id, blks_processed, blks_killed, blks_reused,
			file_extended, lvls_reduced, blks_coalesced, blks_split, blks_swapped);
	return TRUE;
} /* end mu_reorg() */

/**********************************************
 Statistics of reorg for current global.
 Also update dest_blklist_ptr for next globals
***********************************************/
void reorg_finish(block_id dest_blk_id, block_id blks_processed, block_id blks_killed,
		block_id blks_reused, block_id file_extended, block_id lvls_reduced,
		block_id blks_coalesced, block_id blks_split, block_id blks_swapped)
{
	t_abort(gv_cur_region, cs_addrs);
	file_extended = cs_data->trans_hist.total_blks - file_extended;
	util_out_print("Blocks processed    : !@UQ ", FLUSH, &blks_processed);
	util_out_print("Blocks coalesced    : !@UQ ", FLUSH, &blks_coalesced);
	util_out_print("Blocks split        : !@UQ ", FLUSH, &blks_split);
	util_out_print("Blocks swapped      : !@UQ ", FLUSH, &blks_swapped);
	util_out_print("Blocks freed        : !@UQ ", FLUSH, &blks_killed);
	util_out_print("Blocks reused       : !@UQ ", FLUSH, &blks_reused);
	if (0 > lvls_reduced)
	{
		lvls_reduced = -1 * lvls_reduced;
		util_out_print("Levels Increased    : !@UQ ", FLUSH, &lvls_reduced);
	} else if (0 < lvls_reduced)
		util_out_print("Levels Eliminated   : !@UQ ", FLUSH, &lvls_reduced);
	util_out_print("Blocks extended     : !@UQ ", FLUSH, &file_extended);
	cs_addrs->reorg_last_dest = dest_blk_id;
	/* next attempt for this global will start from the beginning, if RESUME option is present */
	cs_data->reorg_restart_block = 0;
	cs_data->reorg_restart_key[0] = 0;
	cs_data->reorg_restart_key[1] = 0;
}
