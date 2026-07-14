/****************************************************************
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/***************************************************************************************************************
 * mu_trunc_tail_sweep.c:
 *	Helper for MUPIP REORG -TRUNCATE (see YDB#1245). The reorg phase of a REORG -TRUNCATE operates on the
 *	list of global names built when the command started. If concurrent updates create new globals (or add
 *	blocks to existing globals after the reorg phase visited them), those blocks can end up towards the end
 *	of the database file, either allocated there directly or displaced there by a block swap done by the
 *	reorg phase ("mu_swap_blk" exchanges the contents of the working block and a busy destination block).
 *	Busy blocks at the end of the file prevent "mu_truncate" from freeing up the space at the end of the
 *	file even though the file may have lots of free space.
 *
 *	"mu_trunc_tail_sweep" scans the local bitmaps covering the "tail" of the database file (the portion
 *	that a maximally successful truncate could free up) for BUSY blocks, determines the global name each
 *	such block belongs to (from the first key in the block, descending past *-key-only index blocks like
 *	"mu_swap_blk" does for a prospective destination block) and then reruns "mu_reorg" and "mu_swap_root"
 *	for those global names. That moves all of the blocks of those globals (data/index blocks via block
 *	swaps, root blocks and directory tree blocks via "mu_swap_root") towards the front of the file. Since
 *	concurrent updates can create new tail blocks while the sweep runs, the scan+reorg is repeated a few
 *	(bounded number of) times. While a sweep is in progress, "mu_trunc_sweep_in_prog" is TRUE, which makes
 *	"mu_swap_blk" only use FREE/RECYCLED blocks as swap destinations (a busy<->busy exchange does not help
 *	compaction and would displace yet another block towards the end of the file, preventing convergence).
 *
 *	The sweep is best effort. Blocks it cannot move (e.g. globals in the -EXCLUDE list, blocks of a global
 *	that is concurrently being killed) are left alone; "mu_truncate" then truncates whatever it can.
 **************************************************************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdskill.h"
#include "gdscc.h"
#include "jnl.h"
#include "copy.h"
#include "muextr.h"
#include "mu_reorg.h"
#include "min_max.h"
/* Include prototypes */
#include "gvcst_protos.h"	/* for GVCST_ROOT_SEARCH */
#include "gvt_inline.h"
#include "gv_trigger.h"
#include "gv_trigger_common.h"
#include "mupip_reorg.h"
#include "mu_truncate.h"	/* for GET_STATUS */
#include "op.h"
#include "t_abort.h"
#include "t_begin.h"
#include "t_qread.h"
#include "t_retry.h"
#include "targ_alloc.h"	/* for the "targ_alloc" call in the SET_GVTARGET_TO_HASHT_GBL macro */
#include "tp_change_reg.h"
#include "util.h"

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	boolean_t		mu_reorg_more_tries;
GBLREF	boolean_t		mu_reorg_process;
GBLREF	boolean_t		mu_trunc_sweep_in_prog;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_namehead		*reorg_gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned char		rdfail_detail;

#ifdef DEBUG
GBLREF	block_id		ydb_skip_bml_num;
#endif

error_def(ERR_MUREORGFAIL);

#define	MU_TRUNC_SWEEP_MAX_NAMES	256	/* names found in one scan; an overflow is caught by the next iteration */
#define	MU_TRUNC_SWEEP_MAX_ITERS	3	/* bounded so sustained concurrent updates cannot make the sweep spin */

typedef struct
{
	unsigned char	name[MAX_MIDENT_LEN + 1];
	int		len;
} trunc_sweep_name;

STATICFNDCL int4 mu_trunc_tail_scan(trunc_sweep_name *names, int4 max_names, glist *exclude_glist_ptr);

/* Scan the local bitmaps covering the tail of the database file for BUSY blocks and record the (deduplicated)
 * global names those blocks belong to. Returns the number of names found. The scan is advisory: it is fine for
 * it to see (transiently) stale bitmap/block contents since the "mu_reorg"/"mu_swap_root" calls that act on the
 * returned names revalidate everything transactionally. Blocks whose name cannot be determined (e.g. blocks of
 * a killed GVT with just a block header, bad-looking keys from a concurrent update) are skipped.
 */
STATICFNDEF int4 mu_trunc_tail_scan(trunc_sweep_name *names, int4 max_names, glist *exclude_glist_ptr)
{
	block_id		child, lmap_blk_num, lmap_num, num_local_maps, start_lmap, target_blks, total_blks;
	boolean_t		long_blk_id, names_full, read_failed, skip_block;
	blk_hdr_ptr_t		blk_hdr_ptr;
	cache_rec_ptr_t		cr, cr1;
	int			bml_status, blk, blks_in_lmap, end_blocks, key_len_dir, name_len, nslevel;
	int			rec_size1;
	int4			cycle, cycle1, i, n_names;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_base, bmp_base, lmap_addr, name_ptr, rec_base, tblk_ptr;
	unsigned short		temp_ushort;	/* needed by the GET_RSIZ macro */

	csa = cs_addrs;
	csd = cs_data;
	total_blks = csa->ti->total_blks;
	/* Any BUSY block below "target_blks" cannot limit the truncate (there are not enough free blocks below it for
	 * the truncate point to go under it), so only scan the local bitmaps at or above it. Note that this is a
	 * heuristic bound computed without crit; it only affects how much gets scanned, not correctness.
	 */
	assert(total_blks >= csa->ti->free_blocks);
	target_blks = total_blks - csa->ti->free_blocks;
	start_lmap = target_blks / BLKS_PER_LMAP;
	num_local_maps = DIVIDE_ROUND_UP(total_blks, BLKS_PER_LMAP);
	/* (total_blks % BLKS_PER_LMAP) can be cast because it should never be larger than BLKS_PER_LMAP */
	end_blocks = (int4)(total_blks % BLKS_PER_LMAP);
	if (0 == end_blocks)
		end_blocks = BLKS_PER_LMAP;
	n_names = 0;
	names_full = FALSE;
	for (lmap_num = num_local_maps - 1; (lmap_num >= start_lmap) && (0 < lmap_num) && !names_full; lmap_num--)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		lmap_blk_num = lmap_num * BLKS_PER_LMAP;
#		ifdef DEBUG
		if ((0 != ydb_skip_bml_num) && (BLKS_PER_LMAP <= lmap_blk_num) && (lmap_blk_num < ydb_skip_bml_num))
			break;
#		endif
		blks_in_lmap = (lmap_num == num_local_maps - 1) ? end_blocks : BLKS_PER_LMAP;
		t_begin(ERR_MUREORGFAIL, 0);
		for (;;)
		{	/* transaction retry loop for scanning this local bitmap */
			bmp_base = t_qread(lmap_blk_num, (sm_int_ptr_t)&cycle, &cr);
			if (NULL == bmp_base)
			{
				t_retry((enum cdb_sc)rdfail_detail);
				continue;
			}
			if (BM_SIZE(BLKS_PER_LMAP) != ((blk_hdr_ptr_t)bmp_base)->bsiz)
			{	/* Buffer no longer holds a bitmap block. Restartable situation. */
				t_retry(cdb_sc_lostcr);
				continue;
			}
			lmap_addr = bmp_base + SIZEOF(blk_hdr);
			read_failed = FALSE;
			for (blk = 1; (blk < blks_in_lmap) && !names_full; blk++)
			{
				GET_STATUS(*(lmap_addr + (blk / BML_BLKS_PER_UCHAR)), (blk % BML_BLKS_PER_UCHAR), bml_status);
				if (BLK_BUSY != bml_status)
					continue;
				blk_base = t_qread(lmap_blk_num + blk, (sm_int_ptr_t)&cycle1, &cr1);
				if (NULL == blk_base)
				{
					read_failed = TRUE;
					break;
				}
				blk_hdr_ptr = (blk_hdr_ptr_t)blk_base;
				if ((LCL_MAP_LEVL == blk_hdr_ptr->levl) || (SIZEOF(blk_hdr) >= blk_hdr_ptr->bsiz)
						|| (blk_hdr_ptr->bsiz > csd->blk_size))
					continue;	/* stale read, or leftover of a killed GVT; skip */
				/* If the block starts with a *-key record, follow child pointers down until a real key
				 * is found (same approach as "mu_swap_blk" uses for a prospective destination block).
				 */
				tblk_ptr = blk_base;
				nslevel = blk_hdr_ptr->levl;
				long_blk_id = IS_64_BLK_ID(tblk_ptr);
				rec_base = tblk_ptr + SIZEOF(blk_hdr);
				GET_RSIZ(rec_size1, rec_base);
				skip_block = FALSE;
				while ((bstar_rec_size(long_blk_id) == rec_size1) && (0 != nslevel))
				{
					READ_BLK_ID(long_blk_id, &child, rec_base + SIZEOF(rec_hdr));
					if ((0 >= child) || (child >= csa->ti->total_blks))
					{
						skip_block = TRUE;
						break;
					}
					tblk_ptr = t_qread(child, (sm_int_ptr_t)&cycle1, &cr1);
					if (NULL == tblk_ptr)
					{
						read_failed = TRUE;
						break;
					}
					long_blk_id = IS_64_BLK_ID(tblk_ptr);
					/* leaf of a killed GVT can have block header only. Skip those blocks */
					if (SIZEOF(blk_hdr) >= ((blk_hdr_ptr_t)tblk_ptr)->bsiz)
					{
						skip_block = TRUE;
						break;
					}
					nslevel--;
					rec_base = tblk_ptr + SIZEOF(blk_hdr);
					GET_RSIZ(rec_size1, rec_base);
				}
				if (read_failed)
					break;
				if (skip_block || (bstar_rec_size(long_blk_id) == rec_size1))
					continue;	/* could not get to a real key; skip this block */
				key_len_dir = get_gblname_len(tblk_ptr, rec_base + SIZEOF(rec_hdr));
				if ((1 >= key_len_dir) || ((MAX_MIDENT_LEN + 1) < key_len_dir))
					continue;	/* likely a just-killed block still marked busy; skip */
				name_ptr = rec_base + SIZEOF(rec_hdr);
				name_len = key_len_dir - 1;
				if ((NULL != exclude_glist_ptr) && (NULL != exclude_glist_ptr->next)
						&& in_exclude_list(name_ptr, name_len, exclude_glist_ptr))
					continue;	/* honor -EXCLUDE: do not move this global's blocks */
				for (i = 0; i < n_names; i++)
					if ((names[i].len == name_len) && (0 == memcmp(names[i].name, name_ptr, name_len)))
						break;
				if (i < n_names)
					continue;	/* already have this name */
				memcpy(names[n_names].name, name_ptr, name_len);
				names[n_names].name[name_len] = '\0';
				names[n_names].len = name_len;
				n_names++;
				if (max_names == n_names)
					names_full = TRUE;	/* stop scanning; next sweep iteration picks up the rest */
			}
			if (read_failed)
			{
				t_retry((enum cdb_sc)rdfail_detail);
				continue;
			}
			break;
		}
		t_abort(gv_cur_region, csa);
	}
	return n_names;
}

/* Sweep the tail of the current region (gv_cur_region/cs_addrs/cs_data must be set up by the caller) so that a
 * following "mu_truncate" call can free up as much space as possible. See comment at the top of this file.
 * The index/data fill factors and reorg_op are passed through to "mu_reorg" so the sweep reorgs stragglers the
 * same way the main reorg phase would have. "truncate_percent" is used to skip the sweep in case "mu_truncate"
 * would not attempt a truncate anyway.
 */
void mu_trunc_tail_sweep(glist *exclude_glist_ptr, int index_fill_factor, int data_fill_factor, int reorg_op,
				int4 truncate_percent)
{
	static trunc_sweep_name	names[MU_TRUNC_SWEEP_MAX_NAMES];
	boolean_t		lcl_resume, progress;
	gd_region		*sweep_reg;
	glist			gl;
	int			root_swap_statistic;
	int4			iter, i, n_names;
	mval			gbl_name_mval;
	sgmnt_addrs		*csa;

	csa = cs_addrs;
	if (dba_mm == cs_data->acc_meth)
		return;	/* "mu_truncate" is going to issue a MUTRUNCNOTBG message; nothing for the sweep to do */
	if (csa->ti->free_blocks < (truncate_percent * csa->ti->total_blks / 100))
		return;	/* "mu_truncate" is going to issue a MUTRUNCNOSPACE message; nothing for the sweep to do */
	sweep_reg = gv_cur_region;
	gv_target = NULL;	/* do not let "t_retry" (if invoked during the scan) act on a stale/wrong-region target;
				 * "op_gvname"/SET_GVTARGET_TO_HASHT_GBL set it before it is actually needed.
				 */
	gv_currkey->end = 0;	/* keep gv_currkey in sync with the NULL gv_target (see the assert in
				 * "dbg_check_gvtarget_gvcurrkey_in_sync" in gvt_inline.h); a stale key could otherwise
				 * be left over from the preceding root block swap phase of mupip_reorg.
				 */
	gv_currkey->base[0] = KEY_DELIMITER;
	/* The mupip_reorg caller reset the below two before the truncate phase but "mu_reorg"/"mu_swap_root" (and the
	 * t_qread calls done by the scan) expect REORG semantics; restore them for the duration of the sweep.
	 */
	mu_reorg_more_tries = mu_reorg_process = TRUE;
	mu_trunc_sweep_in_prog = TRUE;	/* makes "mu_swap_blk" only use FREE/RECYCLED destinations; see comment there */
	for (iter = 1; MU_TRUNC_SWEEP_MAX_ITERS >= iter; iter++)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		n_names = mu_trunc_tail_scan(names, MU_TRUNC_SWEEP_MAX_NAMES, exclude_glist_ptr);
		if (0 == n_names)
			break;	/* tail has no movable busy blocks; nothing (more) to sweep */
		progress = FALSE;
		for (i = 0; i < n_names; i++)
		{
			if (mu_ctrly_occurred || mu_ctrlc_occurred)
				break;
			/* A preceding "mu_reorg"/"mu_swap_root" call (this loop's previous iteration or the caller's
			 * reorg phase) leaves inctn_opcode set to inctn_mu_reorg. Reset it since the root search done
			 * by "op_gvname"/GVCST_ROOT_SEARCH below commits read-only (update_trans == 0) transactions
			 * and "t_end" asserts that those do not have an inctn_opcode set.
			 */
			inctn_opcode = inctn_invalid_op;
			if ('#' == names[i].name[0])
			{	/* A block of the trigger global ^#t. Set up gv_target the same way mupip_reorg does. */
#				ifdef GTM_TRIGGER
				SET_GVTARGET_TO_HASHT_GBL(cs_addrs);	/* sets gv_target */
				SET_GV_CURRKEY_FROM_GVT(gv_target);
				gv_target->root = 0;	/* recompute root in case a concurrent process moved things around */
				inctn_opcode = inctn_invalid_op;	/* needed for GVCST_ROOT_SEARCH */
				GVCST_ROOT_SEARCH;			/* sets gv_target->root */
				if (0 == gv_target->root)
					continue;
#				else
				continue;
#				endif
			} else
			{	/* Bind the name (op_gvname sets gv_target/gv_cur_region/cs_addrs et al). */
				memset(&gbl_name_mval, 0, SIZEOF(mval));
				gbl_name_mval.mvtype = MV_STR;
				gbl_name_mval.str.addr = (char *)names[i].name;
				gbl_name_mval.str.len = names[i].len;
				op_gvname(VARLSTCNT(1) &gbl_name_mval);
				if (gv_cur_region != sweep_reg)
				{	/* The global directory maps this name to a different region even though this
					 * region's file has blocks with its name (possible only with an unusual gld
					 * change). Leave those blocks alone.
					 */
					gv_cur_region = sweep_reg;
					tp_change_reg();
					continue;
				}
				if (0 == gv_target->root)
					continue;	/* global was killed since the scan; its blocks will turn recycled */
			}
			gl.next = NULL;
			gl.reg = sweep_reg;
			gl.gvt = gv_target;
			gl.gvnh_reg = NULL;
			util_out_print("   ", FLUSH);
			util_out_print("Truncate tail sweep of Global: !AD (region !AD)", FLUSH,
					GNAME(&gl).len, GNAME(&gl).addr, REG_LEN_STR(sweep_reg));
			/* Restart the swap destination search from the start of the file so free blocks towards the
			 * front (including those freed by earlier sweep iterations) get used.
			 */
			csa->reorg_last_dest = 0;
			/* Save the global name in reorg_gv_target (see comment in mupip_reorg.c before its mu_reorg call) */
			reorg_gv_target->gvname.var_name = GNAME(&gl);
			lcl_resume = FALSE;
			if (mu_reorg(&gl, exclude_glist_ptr, &lcl_resume, index_fill_factor, data_fill_factor,
					reorg_op, 0))
				progress = TRUE;
			SET_GV_CURRKEY_FROM_GVT(reorg_gv_target);
			root_swap_statistic = 0;
			mu_swap_root(&gl, &root_swap_statistic, 0);
		}
		if (!progress)
			break;	/* avoid spinning if nothing can be moved (e.g. concurrent kills in progress) */
	}
	mu_trunc_sweep_in_prog = FALSE;
	mu_reorg_more_tries = mu_reorg_process = FALSE;
	if (gv_cur_region != sweep_reg)
	{	/* restore the caller's region */
		gv_cur_region = sweep_reg;
		tp_change_reg();
	}
	return;
}
