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
 *	(bounded number of) times. The sweep sets the TRUNC_SWEEP_IN_PROG bit in the reorg_op it passes to
 *	"mu_reorg", which makes "mu_swap_blk" only use FREE/RECYCLED blocks as swap destinations (a busy<->busy
 *	exchange does not help compaction and would displace yet another block towards the end of the file,
 *	preventing convergence). It also sets the NOSPLIT/NOCOALESCE bits so those "mu_reorg" calls do block
 *	swaps only, and passes both "mu_reorg" and "mu_swap_root" the lowest block number worth moving so they
 *	move only those blocks of the global that lie past the truncate point (see comments in
 *	"mu_trunc_tail_sweep" below).
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
#include "toktyp.h"	/* needed by valid_mname.h */
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
#include "valid_mname.h"

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	boolean_t		mu_reorg_more_tries;
GBLREF	boolean_t		mu_reorg_process;
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

STATICFNDCL int4 mu_trunc_tail_scan(trunc_sweep_name *names, int4 max_names, glist *exclude_glist_ptr,
			block_id *max_busy_blk_ptr, block_id *sweep_start_blk_ptr);

/* Scan the local bitmaps covering the tail of the database file for BUSY blocks and record the (deduplicated)
 * global names those blocks belong to. Returns the number of names found. The scan is advisory: it is fine for
 * it to see (transiently) stale bitmap/block contents since the "mu_reorg"/"mu_swap_root" calls that act on the
 * returned names revalidate everything transactionally. Blocks whose name cannot be determined (e.g. blocks of
 * a killed GVT with just a block header, bad-looking keys from a concurrent update) are skipped.
 * "*max_busy_blk_ptr" is set to the highest BUSY block the scan saw (0 if none), INCLUDING blocks it skipped or
 * whose global is in the exclude list: the highest busy block is what caps the achievable truncate point, so the
 * caller uses this to tell whether an iteration made progress (see comment there).
 * "*sweep_start_blk_ptr" is set to the lowest block number this scan considered worth moving (0 if the scan did
 * not run); the caller passes it to "mu_reorg" so only those blocks get swapped (see comment there).
 */
STATICFNDEF int4 mu_trunc_tail_scan(trunc_sweep_name *names, int4 max_names, glist *exclude_glist_ptr,
			block_id *max_busy_blk_ptr, block_id *sweep_start_blk_ptr)
{
	block_id		child, free_blks, lmap_blk_num, lmap_num, max_busy_blk, num_local_maps, start_lmap;
	block_id		target_blks, total_blks;
	boolean_t		long_blk_id, names_full, read_failed, skip_block;
	blk_hdr_ptr_t		blk_hdr_ptr;
	cache_rec_ptr_t		cr, cr1;
	int			bml_status, blk, blks_in_lmap, end_blocks, key_len_dir, name_len, nslevel;
	int			rec_size1;
	int4			cycle, cycle1, i, n_names;
	mstr			name_mstr;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_base, bmp_base, lmap_addr, name_ptr, rec_base, tblk_ptr;
	unsigned short		temp_ushort;	/* needed by the GET_RSIZ macro */

	csa = cs_addrs;
	csd = cs_data;
	*sweep_start_blk_ptr = 0;
	total_blks = csa->ti->total_blks;
	free_blks = csa->ti->free_blocks;
	/* The two crit-free reads above are not atomic. A concurrent file extension (or a truncate done by a MUPIP
	 * REORG -TRUNCATE in another process) between them can make the two values mutually inconsistent, e.g. a
	 * free_blks (read after an extension) greater than total_blks (read before it). In that case skip this scan:
	 * the file is visibly in flux, the sweep is best effort, and the next sweep iteration and/or the following
	 * "mu_truncate" reread fresh values. free_blks equal to total_blks (only possible through the same race, since
	 * bitmap blocks are always BUSY) means nothing is busy, so there is nothing to sweep either way.
	 */
	if (free_blks >= total_blks)
		return 0;
	/* "target_blks" is the number of blocks a maximally compacted database would occupy. "mu_truncate" frees space
	 * at local bitmap granularity (it truncates everything above the highest local bitmap containing a BUSY block),
	 * and the local bitmap containing "target_blks" retains BUSY blocks even under perfect compaction (the blocks
	 * just below "target_blks" have nowhere lower to go). Therefore BUSY blocks in that bitmap (or below) can never
	 * limit the achievable truncation and moving them gains nothing: only scan the local bitmaps lying ENTIRELY
	 * above "target_blks" (i.e. round up, not down). This also means that when the preceding reorg phase compacted
	 * the file without concurrent-update interference, the scan finds nothing and the sweep does no work (and
	 * produces no output). Note that this is a heuristic bound computed without crit; it only affects how much gets
	 * scanned, not correctness.
	 */
	target_blks = total_blks - free_blks;
	start_lmap = DIVIDE_ROUND_UP(target_blks, BLKS_PER_LMAP);
	/* Blocks below the first scanned local bitmap are not worth moving (see comment above). Hand that bound to the
	 * caller so the "mu_reorg" calls it does restrict their block swaps to the same set of blocks this scan looked
	 * at. Note that "start_lmap" is >= 1 here (since "target_blks" is >= 1 given the "free_blks >= total_blks"
	 * check above), so "*sweep_start_blk_ptr" is non-zero i.e. never confused with the "no restriction" value.
	 */
	*sweep_start_blk_ptr = start_lmap * BLKS_PER_LMAP;
	num_local_maps = DIVIDE_ROUND_UP(total_blks, BLKS_PER_LMAP);
	/* (total_blks % BLKS_PER_LMAP) can be cast because it should never be larger than BLKS_PER_LMAP */
	end_blocks = (int4)(total_blks % BLKS_PER_LMAP);
	if (0 == end_blocks)
		end_blocks = BLKS_PER_LMAP;
	n_names = 0;
	names_full = FALSE;
	max_busy_blk = 0;
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
			/* Re-read total_blks each (re)try: a MUPIP REORG -TRUNCATE in another process can truncate the
			 * file while this scan runs (cnl->trunc_pid only serializes the "mu_truncate" phase, which this
			 * process has not reached yet). Reading a block at/above the shrunken total_blks would make
			 * "t_qread" return NULL (cdb_sc_blknumerr) no matter how often it is retried and, once "t_retry"
			 * escalates to the final retry (which grabs crit), fail an assert in "t_qread" (an out-of-range
			 * read while holding crit is otherwise a logic error). The re-read is racy without crit but
			 * converges: every retry re-reads it and in the final retry (crit held) it cannot change.
			 */
			total_blks = csa->ti->total_blks;
			if (lmap_blk_num >= total_blks)
				break;	/* bitmap truncated away; the blocks it covered are past the new end of the file */
			if (blks_in_lmap > (total_blks - lmap_blk_num))
				blks_in_lmap = (int)(total_blks - lmap_blk_num);	/* file now ends inside this bitmap */
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
				if ((lmap_blk_num + blk) > max_busy_blk)
					max_busy_blk = lmap_blk_num + blk;
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
				name_len = key_len_dir - 1;
				memcpy(names[n_names].name, rec_base + SIZEOF(rec_hdr), name_len);
				names[n_names].name[name_len] = '\0';
				name_ptr = names[n_names].name;
				/* The reads above are done from a shared memory buffer without crit, so a concurrent
				 * update can rewrite the buffer between the "get_gblname_len" call and the "memcpy"
				 * (a torn read). Validate the PRIVATE copy before using it: every global name is a
				 * valid M identifier (the trigger global ^#t being the sole exception), whereas a torn
				 * read can produce garbage, e.g. a "name" with an embedded KEY_DELIMITER, which
				 * "op_gvname" (in the sweep loop) must never see.
				 */
				if ('#' == name_ptr[0])
				{	/* a block of the trigger global ^#t (or a torn read) */
					if ((HASHT_GBLNAME_LEN != name_len)
							|| (0 != memcmp(name_ptr, HASHT_GBLNAME, HASHT_GBLNAME_LEN)))
						continue;
				} else
				{
					name_mstr.addr = (char *)name_ptr;
					name_mstr.len = name_len;
					if (!valid_mname(&name_mstr))
						continue;	/* torn read; a later scan sees the settled contents */
				}
				if ((NULL != exclude_glist_ptr) && (NULL != exclude_glist_ptr->next)
						&& in_exclude_list(name_ptr, name_len, exclude_glist_ptr))
					continue;	/* honor -EXCLUDE: do not move this global's blocks */
				for (i = 0; i < n_names; i++)
					if ((names[i].len == name_len) && (0 == memcmp(names[i].name, name_ptr, name_len)))
						break;
				if (i < n_names)
					continue;	/* already have this name */
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
	*max_busy_blk_ptr = max_busy_blk;
	return n_names;
}

/* Sweep the tail of the current region (gv_cur_region/cs_addrs/cs_data must be set up by the caller) so that a
 * following "mu_truncate" call can free up as much space as possible. See comment at the top of this file.
 * The reorg_op flags are passed through to "mu_reorg" (with sweep-specific bits added below). The index/data
 * fill factors are passed through too, though they only matter to the split/coalesce operations the sweep
 * disables. "truncate_percent" is used to skip the sweep in case "mu_truncate" would not attempt a truncate
 * anyway.
 */
void mu_trunc_tail_sweep(glist *exclude_glist_ptr, int index_fill_factor, int data_fill_factor, int reorg_op,
				int4 truncate_percent)
{
	static trunc_sweep_name	names[MU_TRUNC_SWEEP_MAX_NAMES];
	block_id		max_busy_blk, prev_max_busy, sweep_start_blk;
	boolean_t		lcl_resume, progress;
	gd_region		*sweep_reg;
	glist			gl;
	int			root_swap_statistic;
	int4			iter, i, n_names;
	mval			gbl_name_mval;
	sgmnt_addrs		*csa;

	csa = cs_addrs;
	if (reorg_op & NOSWAP)
		return;	/* A block swap is the only operation the sweep does (see NOSPLIT/NOCOALESCE comment below), so
			 * with -NOSWAP the sweep could move nothing; skip even the scan.
			 */
	if (dba_mm == cs_data->acc_meth)
		return;	/* "mu_truncate" is going to issue a MUTRUNCNOTBG message; nothing for the sweep to do */
	if (csa->ti->free_blocks < (truncate_percent * csa->ti->total_blks / 100))
		return;	/* "mu_truncate" is going to issue a MUTRUNCNOSPACE message; nothing for the sweep to do */
	if (SNAPSHOTS_IN_PROG(csa->nl) || (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb))
		return;	/* "mu_truncate" refuses to truncate while an online INTEG (snapshot) or an online BACKUP is in
			 * progress on the region (it issues a MUTRUNCSSINPROG/MUTRUNCBACKINPROG message), so a sweep
			 * would be all cost and no benefit (the block moves it does are extra work for the INTEG/BACKUP
			 * too). Both checks are crit-free reads (like the ones above): a snapshot/backup starting right
			 * after them does not get the sweep skipped, it just means the sweep's work goes unused this
			 * time around.
			 */
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
	/* Note down that the "mu_reorg" calls below are done by a tail sweep. This makes "mu_swap_blk" only use
	 * FREE/RECYCLED destinations; see comment there. Also disable splits and coalesces: a block swap is the only
	 * reorg operation that moves a block towards the front of the file, which is all the sweep is after, whereas
	 * splits/coalesces are block-fill quality operations that would just make the sweep more expensive. (In theory
	 * a coalesce can free a tail block that a swap cannot move -- an under-filled block with no FREE/RECYCLED
	 * destination below it -- but that means the file is packed solid below that block, so there is next to nothing
	 * for "mu_truncate" to reclaim anyway; the sweep is best effort, see comment at the top of this file.)
	 */
	reorg_op |= (TRUNC_SWEEP_IN_PROG | NOSPLIT | NOCOALESCE);
	prev_max_busy = 0;
	for (iter = 1; MU_TRUNC_SWEEP_MAX_ITERS >= iter; iter++)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		n_names = mu_trunc_tail_scan(names, MU_TRUNC_SWEEP_MAX_NAMES, exclude_glist_ptr, &max_busy_blk,
						&sweep_start_blk);
		if (0 == n_names)
			break;	/* tail has no movable busy blocks; nothing (more) to sweep */
		if ((0 != prev_max_busy) && (max_busy_blk >= prev_max_busy))
			break;	/* The highest BUSY block did not move down since the previous iteration. Since the highest
				 * busy block is what caps the achievable truncate point, iterating further cannot improve
				 * the truncate even if lower blocks are movable. This covers both blocks a previous
				 * iteration tried and failed to move (e.g. block swaps found no usable destination) and
				 * blocks the sweep will never move (e.g. a global in the -EXCLUDE list).
				 */
		prev_max_busy = max_busy_blk;
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
				{	/* The global directory maps this (unsubscripted) name to a different region even
					 * though this region's file has blocks with its name (e.g. a global that spans
					 * multiple regions, or a gld that changed since the blocks were created). Leave
					 * those blocks alone. "op_gvname" pointed gv_target/gv_currkey at the other
					 * region so reset them BEFORE restoring cs_addrs to this region or else they
					 * would be out of sync with cs_addrs and fail the DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC
					 * check (in "dbg_check_gvtarget_gvcurrkey_in_sync") done e.g. at the start of the
					 * "op_gvname" call for the next name.
					 */
					gv_target = NULL;
					gv_currkey->end = 0;
					gv_currkey->base[0] = KEY_DELIMITER;
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
			/* Pass "sweep_start_blk" so "mu_reorg" swaps ONLY those blocks of this global that lie past the
			 * truncate point. Without this, a global with just a few blocks in the tail (the norm: user
			 * databases routinely have globals occupying a gigabyte or more, of which the preceding reorg
			 * phase or a concurrent update displaced only a handful of blocks to the tail) would have ALL of
			 * its blocks swapped, which is a lot of needless database updates (each swap is a transaction,
			 * with journal records and before-images) that do nothing for the truncate. "mu_reorg" still walks
			 * the entire global (the traversal is how it finds the tail blocks transactionally) but that is a
			 * read-only cost. See also the comment before the "sweep_start_blk" check in "mu_reorg".
			 */
			if (mu_reorg(&gl, exclude_glist_ptr, &lcl_resume, index_fill_factor, data_fill_factor,
					reorg_op, 0, sweep_start_blk))
				progress = TRUE;
			SET_GV_CURRKEY_FROM_GVT(reorg_gv_target);
			root_swap_statistic = 0;
			/* Pass "sweep_start_blk" here too so the root block of this global and its directory tree
			 * blocks get moved only if they lie past the truncate point (same reasoning as the "mu_reorg"
			 * call above).
			 */
			mu_swap_root(&gl, &root_swap_statistic, 0, sweep_start_blk);
		}
		if (!progress)
			break;	/* avoid spinning if nothing can be moved (e.g. concurrent kills in progress) */
	}
	mu_reorg_more_tries = mu_reorg_process = FALSE;
	/* Do not leave gv_target/gv_currkey pointing to this region's last swept global. The mupip_reorg caller is
	 * about to switch to other regions ("tp_change_reg" changes cs_addrs but not gv_target/gv_currkey), which
	 * would leave gv_target out of sync with cs_addrs and fail the DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC check
	 * (in "dbg_check_gvtarget_gvcurrkey_in_sync" in gvt_inline.h) in whatever global reference happens next.
	 */
	gv_target = NULL;
	gv_currkey->end = 0;
	gv_currkey->base[0] = KEY_DELIMITER;
	if (gv_cur_region != sweep_reg)
	{	/* restore the caller's region */
		gv_cur_region = sweep_reg;
		tp_change_reg();
	}
	return;
}
