/****************************************************************
 *								*
 * Copyright (c) 2012-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2025 YottaDB LLC and/or its subsidiaries.	*
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
/* Include prototypes */
#include "add_inter.h"
#include "change_reg.h"
#include "gdsbml.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search prototype */
#include "gvcst_bmp_mark_free.h"
#include "gvcst_kill_sort.h"
#include "gtmmsg.h"
#include "is_proc_alive.h"
#include "jnl_get_checksum.h"
#include "op.h"
#include "memcoherency.h"
#include "mupip_exit.h"
#include "mupip_reorg.h"
#include "sleep_cnt.h"
#include "t_abort.h"
#include "t_begin.h"
#include "t_create.h"
#include "t_end.h"
#include "t_qread.h"
#include "t_retry.h"
#include "t_write_map.h"
#include "t_write.h"
#include "util.h"
#include "wcs_sleep.h"
#include "mu_upgrade_bmm.h"
#include "t_begin_crit.h"

GBLREF	boolean_t		mu_reorg_process;
GBLREF	boolean_t		need_kip_incr;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_namehead		*reorg_gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	kill_set		*kill_set_tail;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			t_err;
GBLREF	uint4			update_trans;
GBLREF	uint4			update_array_size;
GBLREF	unsigned char		cw_map_depth;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned char		rdfail_detail;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			mu_upgrade_in_prog;

#ifdef DEBUG
GBLREF	block_id		ydb_skip_bml_num;
#endif

error_def(ERR_DBRDONLY);
error_def(ERR_GBLNOEXIST);
error_def(ERR_MAXBTLEVEL);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUREORGFAIL);
error_def(ERR_MUTRUNCNOTBG);
error_def(ERR_REORGUPCNFLCT);

#define RETRY_SWAP		(0)
#define ABORT_SWAP		(-1)

void	mu_swap_root(glist *gl_ptr, int *root_swap_statistic_ptr, block_id upg_mv_block)
{
	block_id		child_blk_id, free_blk_id;
	block_id		save_root;
	boolean_t		killed_global;
	enum cdb_sc		status;
	gv_namehead		*save_targ;
	int			level;
	kill_set		kill_set_list;
	node_local_ptr_t	cnl;
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	sm_uc_ptr_t		child_blk_ptr;
	srch_hist		*dir_hist_ptr, *gvt_hist_ptr;
	trans_num		curr_tn, ret_tn;
	unsigned int		lcl_t_tries;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(mu_reorg_process || upg_mv_block);
	gv_target = gl_ptr->gvt;
	gv_target->clue.end = 0;		/* reset clue since reorg action on later globals might have invalidated it */
	reorg_gv_target->gvname.var_name = gv_target->gvname.var_name;  /* needed by SAVE_ROOTSRCH_ENTRY_STATE */
	dir_hist_ptr = gv_target->alt_hist;
	gvt_hist_ptr = &(gv_target->hist);
	inctn_opcode = inctn_invalid_op;
	if (0 == upg_mv_block)
	{	/* set gv_target/gv_currkey/gv_cur_region/cs_addrs/cs_data to correspond to <globalname,reg> in gl_ptr */
		gv_target->root = 0;		/* reset root so we recompute it in DO_OP_GVNAME below */
		DO_OP_GVNAME(gl_ptr);
	} /* else V6 to V7 upgrade where gen_hist_for_blk() has already done the name lookup */
	csa = cs_addrs;
	cnl = csa->nl;
	csd = cs_data;	/* keep csd up to date; with MM, cs_data can change, and, dereferencing an older copy, cause SIG-11 */
	if (gv_cur_region->read_only)
		return;					/* Cannot proceed for read-only data files */
	if ((0 == upg_mv_block) && (0 != cs_addrs->nl->reorg_upgrade_pid) && (is_proc_alive(cs_addrs->nl->reorg_upgrade_pid, 0)))
	{	/* REORG -UPGRADE cannot run concurrently with TRUNCATE and is higher priority. Stop */
		send_msg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(7) MAKE_MSG_INFO(ERR_REORGUPCNFLCT), 5,
				LEN_AND_LIT("REORG -TRUNCATE"),
				LEN_AND_LIT("MUPIP REORG -UPGRADE in progress"),
				cs_addrs->nl->reorg_upgrade_pid);
		gtm_putmsg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(7) MAKE_MSG_INFO(ERR_REORGUPCNFLCT), 5,
				LEN_AND_LIT("REORG -TRUNCATE"),
				LEN_AND_LIT("MUPIP REORG -UPGRADE in progress"),
				cs_addrs->nl->reorg_upgrade_pid);
		mupip_exit(ERR_MUNOFINISH);
	}
	killed_global = FALSE;
	while (0 == gv_target->root)
	{	/* Global does not "exist" */
		if ((0 != upg_mv_block) && (gv_target->hist.h[1].blk_num == gv_target->hist.h[0].blk_num))
		{
			gv_target->root = gv_target->hist.h[0].blk_num;
			killed_global = TRUE;
			break;
		}
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_GBLNOEXIST, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
		return;					/* must have been an online rollback - no problem, a message suffices */
	}
	if ((dba_mm == csd->acc_meth) && (0 == upg_mv_block))
		 /* return for now without doing any swapping operation because later mu_truncate
		 * is going to issue the MUTRUNCNOTBG message.
		 */
		return;
	SET_GV_ALTKEY_TO_GBLNAME_FROM_GV_CURRKEY;		/* set up gv_altkey to be just the gblname */
	/* ------------ Swap root block of global variable tree --------- */
	if (MUPIP_UPGRADE_IN_PROGRESS != mu_upgrade_in_prog)
		t_begin(ERR_MUREORGFAIL, UPDTRNS_DB_UPDATED_MASK);
	else
		t_begin_crit(ERR_MUREORGFAIL);
	for (;;)
	{
		curr_tn = csa->ti->curr_tn;
		kill_set_list.used = 0;
		if (!killed_global)
		{
			save_root = gv_target->root;
			gv_target->root = csa->dir_tree->root;
			gv_target->clue.end = 0;
			if (cdb_sc_normal != (status = gvcst_search(gv_altkey, dir_hist_ptr)))
			{	/* Assign directory tree path to dir_hist_ptr */
				assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
				gv_target->root = save_root;
				t_retry(status);
				continue;
			}
			gv_target->root = save_root;
			gv_target->clue.end = 0;
			if (cdb_sc_normal != (gvcst_search(gv_currkey, NULL)))
			{	/* Assign global variable tree path to gvt_hist_ptr */
				assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
				t_retry(status);
				continue;
			}
		}
		/* we've already searched the directory tree in op_gvname/t_retry and obtained gv_target->root.
		 * Should restart with gvtrootmod2 if they don't agree. gvcst_root_search is the final arbiter.
		 * Really need that for debug info and also should assert(gv_currkey is global name).
		 */
		free_blk_id = mu_swap_root_blk(gl_ptr, gvt_hist_ptr, dir_hist_ptr, &kill_set_list, curr_tn, upg_mv_block);
		if (RETRY_SWAP == free_blk_id)
			continue;
		else if (ABORT_SWAP == free_blk_id)
			break;
		DECR_KIP(csd, csa, kip_csa);
		*root_swap_statistic_ptr += 1;
		break;
	}
	if (0 != upg_mv_block)	/* MUPIP UPGRADE Moving a GVT root from the top down, all done */
		return;		/* UPGRADE is standalone and does not do REORG's deferred exit check */
	/* ------------ Swap blocks in branch of directory tree --------- */
	for (level = 0; level <= MAX_BT_DEPTH; level++)
	{
		if (MUPIP_UPGRADE_IN_PROGRESS != mu_upgrade_in_prog)
			t_begin(ERR_MUREORGFAIL, UPDTRNS_DB_UPDATED_MASK);
		else
			t_begin_crit(ERR_MUREORGFAIL);
		for (;;)
		{
			curr_tn = csa->ti->curr_tn;
			kill_set_list.used = 0;
			save_root = gv_target->root;
			gv_target->root = csa->dir_tree->root;
			gv_target->clue.end = 0;
			if (cdb_sc_normal != (status = gvcst_search(gv_altkey, dir_hist_ptr)))
			{	/* assign branch path of directory tree into dir_hist_ptr */
				assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
				gv_target->root = save_root;
				t_retry(status);
				continue;
			}
			gv_target->root = save_root;
			gv_target->clue.end = 0;
			if (level >= dir_hist_ptr->depth)
			{	/* done */
				t_abort(gv_cur_region, csa);
				return;
			}
			child_blk_ptr = dir_hist_ptr->h[level].buffaddr;
			child_blk_id = dir_hist_ptr->h[level].blk_num;
			assert(csa->dir_tree->root != child_blk_id);
			free_blk_id = swap_root_or_directory_block(level + 1, level, dir_hist_ptr, child_blk_id,
					child_blk_ptr, &kill_set_list, curr_tn, 0);
			if (level == 0)
				/* set level as 1 to mark this kill set is for level-0 block in directory tree.
				 * The kill-set level later will be used in gvcst_bmp_markfree to assign a special value to
				 * cw_set_element, which will be eventually used by t_end to write the block to snapshot
				 */
				kill_set_list.blk[kill_set_list.used - 1].level = 1;
			if (RETRY_SWAP == free_blk_id)
				continue;
			else if (ABORT_SWAP == free_blk_id)
				break;
			update_trans = UPDTRNS_DB_UPDATED_MASK;
			inctn_opcode = inctn_mu_reorg;
			assert(1 == kill_set_list.used);
			need_kip_incr = TRUE;
			if (!csa->now_crit)
				WAIT_ON_INHIBIT_KILLS(cnl, MAXWAIT2KILL);
			DEBUG_ONLY(lcl_t_tries = t_tries);
			TREF(in_mu_swap_root_state) = MUSWP_DIRECTORY_SWAP;
			if ((trans_num)0 == t_end(dir_hist_ptr, NULL, TN_NOT_SPECIFIED))
			{
				TREF(in_mu_swap_root_state) = MUSWP_NONE;
				need_kip_incr = FALSE;
				assert(NULL == kip_csa);
				continue;
			}
			TREF(in_mu_swap_root_state) = MUSWP_NONE;
			gvcst_kill_sort(&kill_set_list);
			TREF(in_mu_swap_root_state) = MUSWP_FREE_BLK;
			GVCST_BMP_MARK_FREE(&kill_set_list, ret_tn, inctn_mu_reorg, inctn_bmp_mark_free_mu_reorg,
					inctn_opcode, csa);
			TREF(in_mu_swap_root_state) = MUSWP_NONE;
			DECR_KIP(csd, csa, kip_csa);
			break;
		}
	}
	DEFERRED_EXIT_REORG_CHECK;	/* a single directory tree has to be quick, so check at end, rather than each DECR_KIP  */
	return;
}

block_id mu_swap_root_blk(glist *gl_ptr, srch_hist *gvt_hist_ptr, srch_hist *dir_hist_ptr, kill_set *kill_set_list,
		trans_num curr_tn, block_id upg_mv_block)
{
	block_id		free_blk_id, root_blk_id;
	boolean_t		tn_aborted;
	int			root_blk_lvl;
	sm_uc_ptr_t		root_blk_ptr;
	trans_num		ret_tn;
	unsigned int		lcl_t_tries;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	root_blk_lvl = gvt_hist_ptr->depth;
	assert(0 < root_blk_lvl);
	root_blk_ptr = gvt_hist_ptr->h[root_blk_lvl].buffaddr;
	root_blk_id = gvt_hist_ptr->h[root_blk_lvl].blk_num;
	assert((CDB_STAGNATE > t_tries) || (gv_target->root == gvt_hist_ptr->h[root_blk_lvl].blk_num));
	free_blk_id = swap_root_or_directory_block(0, root_blk_lvl, dir_hist_ptr, root_blk_id,
						   root_blk_ptr, kill_set_list, curr_tn, upg_mv_block);
	if ((RETRY_SWAP == free_blk_id) || (ABORT_SWAP == free_blk_id))
		return free_blk_id;
	assert(ABORT_SWAP < free_blk_id);
	update_trans = UPDTRNS_DB_UPDATED_MASK;
	inctn_opcode = inctn_mu_reorg;
	/* There is either a kill/set list OR the code is moving the root block */
	assert((1 == kill_set_list->used) || ((DIR_ROOT == gv_target->root) && (1 == gv_target->hist.h[1].blk_num)));
	need_kip_incr = TRUE;
	if (!cs_addrs->now_crit)
		WAIT_ON_INHIBIT_KILLS(cs_addrs->nl, MAXWAIT2KILL);
	DEBUG_ONLY(lcl_t_tries = t_tries);
	TREF(in_mu_swap_root_state) = MUSWP_INCR_ROOT_CYCLE;
	assert(!TREF(in_gvcst_redo_root_search));
	if ((trans_num)0 == t_end(gvt_hist_ptr, (DIR_ROOT != root_blk_id) ? dir_hist_ptr : NULL, TN_NOT_SPECIFIED))
	{
		TREF(in_mu_swap_root_state) = MUSWP_NONE;
		need_kip_incr = FALSE;
		assert(NULL == kip_csa);
		ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
		if (tn_aborted)
		{	/* It is not an error if the global (that once existed) doesn't exist anymore (due to ROLLBACK) */
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_GBLNOEXIST, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
			return curr_tn;
		}
		return RETRY_SWAP;
	}
	TREF(in_mu_swap_root_state) = MUSWP_NONE;
	/* Note that this particular process's csa->root_search_cycle is now behind cnl->root_search_cycle.
	 * This forces a cdb_sc_gvtrootmod2 restart in gvcst_bmp_mark_free below.
	 */
	assert(cs_addrs->nl->root_search_cycle > cs_addrs->root_search_cycle);
	if (0 != upg_mv_block)
		cs_addrs->root_search_cycle = cs_addrs->nl->root_search_cycle;
	gvcst_kill_sort(kill_set_list);
	GVCST_BMP_MARK_FREE(kill_set_list, ret_tn, inctn_mu_reorg, inctn_bmp_mark_free_mu_reorg, inctn_opcode, cs_addrs);
	return free_blk_id;
}

/* Finds a free block and adds information to update array and cw_set */
block_id swap_root_or_directory_block(int parent_blk_lvl, int child_blk_lvl, srch_hist *dir_hist_ptr, block_id child_blk_id,
		sm_uc_ptr_t child_blk_ptr, kill_set *kill_set_list, trans_num curr_tn, block_id upg_mv_block)
{
	blk_segment		*bs1, *bs_ptr;
	block_id		hint_blk_num, free_blk_id, total_blks, num_local_maps, master_bit,
				free_bit, temp_blk;
	boolean_t		free_blk_recycled, parent_long_blk_id;
	cw_set_element		*tmpcse;
	int			blk_seg_cnt, blk_size;
	int			parent_blk_size, child_blk_size, bsiz;
	int			rec_size1, curr_offset, bpntr_end, hdr_len;
	int			tmp_cmpc, parent_blk_id_sz;
	int4			hint_bit, maxbitsthismap;
	jnl_buffer_ptr_t	jbbp; /* jbbp is non-NULL only if before-image journaling */
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	sm_uc_ptr_t		parent_blk_ptr, bn_ptr, saved_blk;
	srch_blk_status		bmlhist, freeblkhist;
	unsigned char		save_cw_set_depth;
	unsigned short		temp_ushort;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csd = cs_data;
	csa = cs_addrs;
	blk_size = csd->blk_size;
	/* Find a free/recycled block for new block location. */
	hint_blk_num = upg_mv_block;
	total_blks = csa->ti->total_blks;
	num_local_maps = DIVIDE_ROUND_UP(total_blks, BLKS_PER_LMAP);
	master_bit = bmm_find_free((hint_blk_num / BLKS_PER_LMAP), csa->bmm, num_local_maps);
	if ((NO_FREE_SPACE == master_bit))
	{
		assert(0 == upg_mv_block);
		t_abort(gv_cur_region, csa);
		return ABORT_SWAP;
	}
	bmlhist.blk_num = master_bit * BLKS_PER_LMAP;
	if (NULL == (bmlhist.buffaddr = t_qread(bmlhist.blk_num, (sm_int_ptr_t)&bmlhist.cycle, &bmlhist.cr)))
	{	/* WARNING: assignment above */
		assert(0 == upg_mv_block);
		assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
		t_retry((enum cdb_sc)rdfail_detail);
		return RETRY_SWAP;
	}
	hint_bit = (0 != upg_mv_block) ? ((upg_mv_block % BLKS_PER_LMAP) - 1) : 0;
	/* (total_blks - bmlhist.blk_num) can be cast because it should never be larger then BLKS_PER_LMAP */
	assert(((master_bit + 1) < num_local_maps) || (BLKS_PER_LMAP >= (total_blks - bmlhist.blk_num)));
	maxbitsthismap = (master_bit != (num_local_maps - 1)) ? BLKS_PER_LMAP : (int4)(total_blks - bmlhist.blk_num);
	free_bit = bm_find_blk(hint_bit, bmlhist.buffaddr + SIZEOF(blk_hdr), maxbitsthismap, &free_blk_recycled);
	if (NO_FREE_SPACE == free_bit)
	{	/* This is a case where the master bitmap indicated this local bitmap had free space but the bitmap
		 * has no free space. This is possible if a concurrent process in "bg_update_phase2()" that is updating
		 * this exact same bitmap block has done a gvcst_map_build() call that marked all blocks in the bitmap
		 * as allocated/used but BEFORE the bm_update() call happened a few instructions later. This is a
		 * restartable situation.
		 */
		assert(0 == upg_mv_block);
		assert(t_tries < CDB_STAGNATE);
		t_retry(cdb_sc_bmlmod);
		return RETRY_SWAP;
	}
	free_blk_id = bmlhist.blk_num + free_bit;
	assert((0 == upg_mv_block) || (upg_mv_block <= free_blk_id));
	if (DIR_ROOT >= free_blk_id)
	{	/* Bitmap block 0 and directory tree root block 1 should always be marked busy. */
		assert(0 == upg_mv_block);
		assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
		t_retry(cdb_sc_badbitmap);
		return RETRY_SWAP;
	}
	if ((child_blk_id <= free_blk_id) && (0 == upg_mv_block))
	{	/* stop swapping root or DT blocks once the database is truncated well enough. A good heuristic for this is
		* to check if the block is to be swapped into a higher block number and if so do not swap
		*/
		t_abort(gv_cur_region, csa);
		return ABORT_SWAP;
	}
	/* ====== begin update array ======
	 * Four blocks get changed.
	 * 	1. Free block becomes busy and gains the contents of child (root block/directory tree block)
	 * 	2. Parent block in directory tree remains busy, but points to new root block location.
	 *	3. Free block's corresponding bitmap reflects above change.
	 * 	4. Child block gets marked recycled in bitmap. (GVCST_BMP_MARK_FREE)
	 */
	CHECK_AND_RESET_UPDATE_ARRAY;
	if (free_blk_recycled)
	{	/* Otherwise, it's a completely free block, in which case no need to read. */
		freeblkhist.blk_num = free_blk_id;
		if (NULL == (freeblkhist.buffaddr = t_qread(free_blk_id, (sm_int_ptr_t)&freeblkhist.cycle, &freeblkhist.cr)))
		{
			assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
			t_retry((enum cdb_sc)rdfail_detail);
			return RETRY_SWAP;
		}
	}
	child_blk_size = ((blk_hdr_ptr_t)child_blk_ptr)->bsiz;
	BLK_INIT(bs_ptr, bs1);
	BLK_ADDR(saved_blk, child_blk_size, unsigned char);
	memcpy(saved_blk, child_blk_ptr, child_blk_size);
	BLK_SEG(bs_ptr, saved_blk + SIZEOF(blk_hdr), child_blk_size - SIZEOF(blk_hdr));
	assert(blk_seg_cnt == child_blk_size);
	if (!BLK_FINI(bs_ptr, bs1))
	{
		assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
		t_retry(cdb_sc_blkmod);
		return RETRY_SWAP;
	}
	tmpcse = &cw_set[cw_set_depth];
	(free_blk_recycled) ? BIT_SET_RECYCLED_AND_CLEAR_FREE(tmpcse->blk_prior_state)
			    : BIT_CLEAR_RECYCLED_AND_SET_FREE(tmpcse->blk_prior_state);
	t_create(free_blk_id, bs1, 0, 0, child_blk_lvl);
	SET_CSE_MODE_TO_GDS_T_ACQUIRED(tmpcse);
	tmpcse->ondsk_blkver = ((blk_hdr_ptr_t)child_blk_ptr)->bver; /* Retain block version */
	if (!free_blk_recycled)
		tmpcse->old_block = NULL;
	else
	{
		tmpcse->old_block = freeblkhist.buffaddr;
		tmpcse->cr = freeblkhist.cr;
		tmpcse->cycle = freeblkhist.cycle;
		jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
		if ((NULL != jbbp) && (((blk_hdr_ptr_t)tmpcse->old_block)->tn < jbbp->epoch_tn))
		{
			bsiz = ((blk_hdr_ptr_t)(tmpcse->old_block))->bsiz;
			if (bsiz > blk_size)
			{
				assert(CDB_STAGNATE > t_tries);
				t_retry(cdb_sc_lostbmlcr);
				return RETRY_SWAP;
			}
			JNL_GET_CHECKSUM_ACQUIRED_BLK(tmpcse, csd, csa, tmpcse->old_block, bsiz);
		}
	}
	if (DIR_ROOT != child_blk_id)
	{
		/* 2. Parent block in directory tree remains busy, but points to new child block location. */
		parent_blk_ptr = dir_hist_ptr->h[parent_blk_lvl].buffaddr; /* 0 == parent_blk_lvl if moving a gvt root block */
		parent_long_blk_id = IS_64_BLK_ID(parent_blk_ptr);
		parent_blk_id_sz = SIZEOF_BLK_ID(parent_long_blk_id);
		curr_offset = dir_hist_ptr->h[parent_blk_lvl].curr_rec.offset;
		parent_blk_size = ((blk_hdr_ptr_t)parent_blk_ptr)->bsiz;
		GET_RSIZ(rec_size1, (parent_blk_ptr + curr_offset));
		if ((parent_blk_size < rec_size1 + curr_offset) || (bstar_rec_size(parent_long_blk_id) > rec_size1))
		{
			assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
			t_retry(cdb_sc_blkmod);
			return RETRY_SWAP;
		}
		BLK_INIT(bs_ptr, bs1);
		if (0 == parent_blk_lvl)
			/* There can be collation stuff in the record value after the block pointer. See gvcst_root_search. */
			hdr_len = SIZEOF(rec_hdr) + gv_altkey->end + 1 - EVAL_CMPC((rec_hdr_ptr_t)(parent_blk_ptr + curr_offset));
		else
			hdr_len = rec_size1 - parent_blk_id_sz;
		bpntr_end = curr_offset + hdr_len + parent_blk_id_sz;
		BLK_SEG(bs_ptr, parent_blk_ptr + SIZEOF(blk_hdr), curr_offset + hdr_len - SIZEOF(blk_hdr));
		BLK_ADDR(bn_ptr, parent_blk_id_sz, unsigned char);
		WRITE_BLK_ID(parent_long_blk_id, free_blk_id, bn_ptr);
		BLK_SEG(bs_ptr, bn_ptr, parent_blk_id_sz);
		BLK_SEG(bs_ptr, parent_blk_ptr + bpntr_end, parent_blk_size - bpntr_end);
		assert(blk_seg_cnt == parent_blk_size);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert((0 == upg_mv_block) && (t_tries < CDB_STAGNATE));
			t_retry(cdb_sc_blkmod);
			return RETRY_SWAP;
		}
		t_write(&dir_hist_ptr->h[parent_blk_lvl], bs1, 0, 0, parent_blk_lvl, FALSE, TRUE, GDS_WRITE_KILLTN);
		/* To indicate later snapshot file writing process during fast_integ must write the block to snapshot file */
		BIT_SET_DIR_TREE(cw_set[cw_set_depth - 1].blk_prior_state);
	}
	/* 3. Free block's corresponding bitmap reflects above change. */
	PUT_BLK_ID(update_array_ptr, free_bit);
	save_cw_set_depth = cw_set_depth; /* Bit maps go on end of cw_set (more fake acquired) */
	assert(!cw_map_depth);
	t_write_map(&bmlhist, (block_id *)update_array_ptr, curr_tn, 1);
	cw_map_depth = cw_set_depth;
	cw_set_depth = save_cw_set_depth;
	update_array_ptr += SIZEOF(block_id);
	temp_blk = 0;
	PUT_BLK_ID(update_array_ptr, temp_blk);
	update_array_ptr += SIZEOF(block_id);
	assert(1 == cw_set[cw_map_depth - 1]. reference_cnt);
	/* 4. Child block gets marked recycled in bitmap. (GVCST_BMP_MARK_FREE) */
	kill_set_list->blk[kill_set_list->used].flag = 0;
	kill_set_list->blk[kill_set_list->used].level = 0;
	kill_set_list->blk[kill_set_list->used++].block = child_blk_id;
	return free_blk_id;
}
