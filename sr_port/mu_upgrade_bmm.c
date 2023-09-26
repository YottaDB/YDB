/****************************************************************
 *								*
 * Copyright (c) 2021-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_fcntl.h"	/* Needed for AIX's silly open to open64 translations */
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gds_rundown.h"
#include "gtm_string.h"
#include "gtm_common_defs.h"
#include "util.h"
#include "filestruct.h"
#include "cli.h"
#include "mu_reorg.h"
#include "muextr.h"
#include "memcoherency.h"
#include "sleep_cnt.h"
#include "interlock.h"
#include "hashtab_mname.h"
#include "wcs_flu.h"
#include "jnl.h"

/* Prototypes */
#include "gvnh_spanreg.h"
#include "mu_getlst.h"
#include "mu_rndwn_file.h"
#include "mu_upgrade_bmm.h"
#include "mupip_exit.h"
#include "t_qread.h"
#include "targ_alloc.h"
#include "change_reg.h"
#include "t_abort.h"
#include "t_begin_crit.h"
#include "t_create.h"
#include "t_write.h"
#include "t_write_map.h"
#include "t_end.h"
#include "t_retry.h"
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */
#include "db_header_conversion.h"
#include "anticipatory_freeze.h"
#include "gdsfilext.h"
#include "mupip_reorg.h"
#include "gvcst_bmp_mark_free.h"
#include "mu_gv_cur_reg_init.h"
#include "db_ipcs_reset.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "wcs_mm_recover.h"	/* For CHECK_MM_DBFILEXT_REMAP_IF_NEEDED */
#include "wcs_recover.h"
#include "gvt_inline.h"		/* Before gtmio.h, which includes the open->open64 macro on AIX, which we don't want here. */
#include "gtmio.h"
#include "clear_cache_array.h"
#include "bit_clear.h"
#include "bit_set.h"
#include "gds_blk_upgrade.h"
#include "spec_type.h"
#include "dse.h"
#include "getfree_inline.h"
#include "gvcst_kill_sort.h"
#include "mu_updwn_ver_inline.h"
#include "mucblkini.h"
#include "gv_trigger.h"
#include "gv_trigger_common.h"
#include "cws_insert.h"

#define LEVEL_CNT		0

LITREF	char			*gtm_dbversion_table[];

GBLREF	boolean_t		debug_mupip;
GBLREF	boolean_t		mu_reorg_in_swap_blk, mu_reorg_process, need_kip_incr;
GBLREF	char			*update_array, *update_array_ptr;	/* for the BLK_* macros */
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey, *gv_currkey, *gv_currkey_next_reorg;
GBLREF	gv_namehead		*gv_target, *gv_target_list, *reorg_gv_target, *upgrade_gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	int4			blks_needed, gv_keysize;
GBLREF	sgmnt_addrs		*cs_addrs, *kip_csa;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	tp_region		*grlist;
GBLREF	trans_num		start_tn;
GBLREF	uint4			mu_upgrade_in_prog;		/* 1 indicates MUPIP UPGRADE in progress */
GBLREF	uint4			update_array_size;		/* for the BLK_* macros */
GBLREF	uint4			update_trans;
GBLREF	uint4			process_id;
GBLREF	unsigned char		cw_map_depth, rdfail_detail, t_fail_hist[];
GBLREF	unsigned int		t_tries;
GBLREF	int4			cws_reorg_remove_index;
GBLREF	boolean_t		mu_reorg_more_tries;
GBLREF	block_id		mu_upgrade_pin_blkarray[MAX_BT_DEPTH + 1];
GBLREF	int			mu_upgrade_pin_blkarray_idx;

#define	GVT_HIST_CWS_INSERT(HIST)						\
{										\
	srch_blk_status		*tmpHist;					\
										\
	for (tmpHist = (HIST)->h; tmpHist->blk_num; tmpHist++)			\
		CWS_INSERT(tmpHist->blk_num);					\
}

#define	MU_UPGRADE_PIN_BLK(CURR_BLK, SAVE_CW_STAGNATE_COUNT)				\
{											\
	assert(ARRAYSIZE(mu_upgrade_pin_blkarray) > mu_upgrade_pin_blkarray_idx);	\
	DEBUG_ONLY(SAVE_CW_STAGNATE_COUNT = cw_stagnate.count);				\
	mu_upgrade_pin_blkarray[mu_upgrade_pin_blkarray_idx++] = CURR_BLK;		\
}

#define	MU_UPGRADE_UNPIN_BLK(CURR_BLK, SAVE_CW_STAGNATE_COUNT)				\
{											\
	assert(0 < mu_upgrade_pin_blkarray_idx);					\
	mu_upgrade_pin_blkarray_idx--;							\
	assert(SAVE_CW_STAGNATE_COUNT >= cw_stagnate.count);				\
	delete_hashtab_int8(&cw_stagnate, (ublock_id *)&CURR_BLK);			\
}

static gtm_int8	blk_moved_cnt, killed_gbl_cnt, root_moved_cnt, tot_dt, tot_kill_block_cnt, tot_kill_byte_cnt, tot_levl_cnt,
	tot_splt_cnt;

error_def(ERR_CPBEYALLOC);
error_def(ERR_DBDSRDFMTCHNG);
error_def(ERR_DBFILERR);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUNOUPGRD);
error_def(ERR_MUREORGFAIL);
error_def(ERR_MUSTANDALONE);
error_def(ERR_TEXT);

/******************************************************************************************
 * Takes a pointer to a region and moves blocks in the way of a master map expansion -
 * exact amount varies based on DB parameters; the function assumes that it has standalone
 * access to the the region that it is working on.
 *
 * Input Parameters:
 *	reg: region to be modified
 *	blocks_needed: in addition to blocks needed for the master map shift - might be 0
 * Output Parameters:
 *	Returns SS_NORMAL if successful or an error condition otherwise
 ******************************************************************************************/
int4	mu_upgrade_bmm(gd_region *reg, size_t blocks_needed)
{
	boolean_t		dt, is_bg, is_mm, mv_blk_err;
	block_id		blks_in_way, curbml, new_blk_num, offset, old_blk_num, t_blk_num, old_vbn;
	block_id_32		lost;
	blk_hdr			blkHdr;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr, child_cr = NULL;
	gtm_int8		bmm_byte_adjust, extend;
	gvnh_reg_t		*gvnh_reg = NULL;
	gv_namehead		*gvt;
	int			*blks_to_mv_levl_ptr[MAX_BT_DEPTH], currKeySize, cycle, i,
				key_cmpc, key_len, lev, level, rec_sz, save_errno, *skipped_killed_ptr = NULL;
	int4			blk_size, blks_in_bml, bml_index, bml_status, bmls_to_work, index, num_blks_mv,
				new_bmm_size, status;
	kill_set		kill_set_list;
	mname_entry		gvname;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blkBase, blkBase2, bmm_base, bml_buff, recBase;
	srch_blk_status		*blkhist, hist, *t1;
	srch_hist		*dir_hist_ptr;
	trans_num		ret_tn;
	unix_db_info		*udi;
	unsigned char		*cp, gname[sizeof(mident_fixed) + 2], key_buff[MAX_KEY_SZ + 3];
	char			*bml_lcl_buff;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Setup and check that the region is capable of enlarging the master bitmap */
	csa = cs_addrs;
	csd = cs_data;
	blk_size = csd->blk_size;
	old_vbn = csd->start_vbn;
	start_tn = csd->trans_hist.curr_tn;
	util_out_print("Region !AD : MUPIP UPGRADE -MASTERMAP started.", TRUE, REG_LEN_STR(reg));
	/* Note: journaling is off and before imaging ignored as this version change boundry requires journal restart */
	assert(START_VBN_V6 >= csd->start_vbn);
	is_bg = (dba_bg == reg->dyn.addr->acc_meth);
	if ((is_mm = (dba_mm == reg->dyn.addr->acc_meth))) /* WARNING assignment */
	{	/* MM: Re-align cached GV targets because they weren't setup for gv_cur_region */
		for (gvt = gv_target_list; NULL != gvt; gvt = gvt->next_gvnh)
		{
			gvt->clue.end = 0;	/* Resetting to zero forces a refetch */
			gvt->gd_csa = cs_addrs;	/* UPGRADE overrides all names to be in the current region */
		}
	}
	/* Calculate the number of blocks that need to move. Ideally, the following calculation would give the total bytes to move
	 * 	DISK_BLOCK_SIZE * (START_VBN_CURRENT - csd->start_vbn)
	 * and the code simply moves all the trailing data "up". When "all the trailing data" is on the order of hundreds of MiB
	 * the IO is immense. The solution moves data (blocks) targetting the local bitmap boundaries, local bitmap + 511 blocks,
	 * to make space for the enlarged bitmap.
	 *
	 * While a newly minted V7 database has the starting VBN at 8193, an upgraded database will have a value greater than or
	 * equal to 8193. The new starting vbn needs to fall on an existing local bitmap. There is an additional requirement in
	 * the form of alignment in the bitmap of maps (bmm) or the master map. Each byte in the bitmap of maps identifies 8
	 * local bitmaps, or BITS_PER_UCHAR / MASTER_MAP_BITS_PER_LMAP.
	 *
	 * The space calculation becomes a function of the number of blocks rounded up to local map boundaries which is then
	 * rounded up to a multiple of 8 so that the first local bitmap starts at the first bit in the master bitmap
	 * 	total_bytes		= DISK_BLOCK_SIZE * (START_VBN_CURRENT - csd->start_vbn)
	 * 	number_of_blocks	= total_bytes / csd->blk_size
	 * 	bmls_to_work		= ROUND_UP2(ROUND_UP2(number_of_blocks, BLKS_PER_LMAP), 8)
	 * 	number_of_blocks	= bmls_to_work * BLKS_PER_LMAP
	 *
	 * There is one additional wrinkle. Block 1 is special. The "existing Block 1" must end up at the "new Block 1". The logic
	 * below skips over the "existing Block 1" in the first local bitmap and iterates into the first block of the local bitmap
	 * just beyond the range of local bitmaps to move.
	 *
	 * blks_in_way - Total number of blocks in area being moved (including non-BUSY blocks)
	 * num_blks_mv - number of blocks actually being moved (only moving BUSY blocks)
	 * bmls_to_work - number of local bitmaps in the area being moved
	 */
	blks_in_way = START_VBN_CURRENT - csd->start_vbn;
	blks_in_way = ROUND_UP2((blks_in_way / (blk_size / DISK_BLOCK_SIZE)), BLKS_PER_LMAP);
	bmls_to_work = blks_in_way / BLKS_PER_LMAP;				/* on a small DB this is overkill, but simpler */
	bmls_to_work = ROUND_UP2(bmls_to_work, BITS_PER_UCHAR / MASTER_MAP_BITS_PER_LMAP);
	blks_in_way = bmls_to_work * BLKS_PER_LMAP;
	assert((csd->start_vbn + (blks_in_way * blk_size) / DISK_BLOCK_SIZE) >= START_VBN_CURRENT);
	/* Extremely pessimistic view of the number of blocks needed. Note that the requested amount is the sum of the blocks
	 * needed to move for the master map (bitmap of local maps, bmm) plus the guesstimate of double the number of index
	 * blocks in the database. Should there not be enough blocks, extend the database by that much. */
	blocks_needed += blks_in_way;
	blocks_needed += DIVIDE_ROUND_UP(blocks_needed, BLKS_PER_LMAP);		/* local bit map overhead */
	/* Do not count the free blocks if the database has fewer blocks than needed to move, do not reduce the extension request */
	extend = blocks_needed - ((blocks_needed < csd->trans_hist.total_blks) ? csd->trans_hist.free_blocks : 0);
	if ((0 < extend) && (SS_NORMAL != (status = upgrade_extend(extend, reg))))	/* WARNING assignment */
		return status;	/* extend needed, but failed - driver will try the next region */
	util_out_print("Region !AD : Continuing with master bitmap extension.", TRUE, REG_LEN_STR(reg));
	mu_upgrade_in_prog = MUPIP_UPGRADE_IN_PROGRESS;
	assert(csd->trans_hist.free_blocks >= extend);
	/* Find list of busy blocks to relocate from the start of the database to elsewhere. Organize by level */
	memset(blks_to_mv_levl_ptr, 0, MAX_BT_DEPTH * SIZEOF(*blks_to_mv_levl_ptr));
	mv_blk_err = FALSE;
	memset(&hist, 0, sizeof(hist));
	blkhist = &hist;	/* Block history stand-in */
	bml_lcl_buff = malloc(BM_SIZE(BLKS_PER_LMAP));
	for (curbml = 0, index = old_blk_num = 1; old_blk_num < blks_in_way; curbml += BLKS_PER_LMAP)
	{	/* check which blocks need to move */
		bml_buff = t_qread(curbml, (sm_int_ptr_t)&cycle, &cr);
		if (NULL == bml_buff)
		{	/* read failed */
			assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
			mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
<<<<<<< HEAD
			util_out_print("Region !AD: Read of Bit map @x!@XQ failed.", TRUE, REG_LEN_STR(reg), &curbml);
			util_out_print("Region !AD: not upgraded", TRUE,REG_LEN_STR(reg));
			free(bml_lcl_buff);
=======
			util_out_print("Region !AD : Read of Bit map @x!@XQ failed.", TRUE, REG_LEN_STR(reg), &curbml);
			util_out_print("Region !AD : Not upgraded.", TRUE, REG_LEN_STR(reg));
>>>>>>> fdfdea1e (GT.M V7.1-002)
			return ERR_MUNOFINISH;
		}
		memcpy(bml_lcl_buff, bml_buff, BM_SIZE(BLKS_PER_LMAP));
		blks_in_bml = (blks_in_way > curbml) ? BLKS_PER_LMAP : 2;		/* in last bml only interested in block 1 */
		for (bml_index = 1; bml_index < blks_in_bml; bml_index++)		/* skip bml_index = 0 - the bml itself */
		{	/* process the local bit map for BUSY blocks */
			old_blk_num = curbml + bml_index;
			GET_BM_STATUS(bml_lcl_buff, bml_index, bml_status);
			assert(BLK_MAPINVALID != bml_status);
			if (BLK_BUSY != bml_status)
				continue;
			/* else (BLK_BUSY == bml_status), this block is BUSY so add it to the array of blocks to move */
			index++;
			if (1 == old_blk_num)
				continue;	/* Dealing with the root of the directory tree is special, but count it */
			blkBase = t_qread(old_blk_num, &blkhist->cycle, &blkhist->cr);
			if (NULL == blkBase)
			{	/* read failed */
				assert(blkBase);
				assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
				mv_blk_err = TRUE;
				util_out_print("Region !AD : Read of block @x!@XQ failed; moving on to next block.", TRUE,
					REG_LEN_STR(reg), &old_blk_num);
				break;
			}
			blkHdr = *((blk_hdr_ptr_t)blkBase);
			level = (int)blkHdr.levl;
			if (NULL == blks_to_mv_levl_ptr[level])
			{	/* allocate pointers for an additional level */
				blks_to_mv_levl_ptr[level] = malloc(SIZEOF(int) * 2 * blks_in_way + 1);
				memset(blks_to_mv_levl_ptr[level], 0, SIZEOF(int) * 2 * blks_in_way + 1);
			}
			blks_to_mv_levl_ptr[level][LEVEL_CNT]++;
			blks_to_mv_levl_ptr[level][old_blk_num] = index;
		}	/* end of loop over a local bit map */
	}	/* end identification of blocks to move */
	free(bml_lcl_buff);
	/* crit management is not critical as this runs standalone */
	gvname.var_name.addr = (char *)gname;
	blk_moved_cnt = root_moved_cnt = 0;
	num_blks_mv = index;
	assert(((blks_in_way) + 1) >= old_blk_num);
	assert(DBKEYSIZE(MAX_KEY_SZ) == gv_keysize);			/* gv_keysize was init'ed by gvinit() in the caller */
	if (NULL == gv_currkey_next_reorg)
		GVKEY_INIT(gv_currkey_next_reorg, gv_keysize);
	else	/* Multi-region upgrade in progress, don't reallocate */
		gv_currkey_next_reorg->end = 0;
	upgrade_gv_target = targ_alloc(MAX_KEY_SZ, NULL, NULL);		/* UPGRADE needs space for gen_hist_for_blk */
	upgrade_gv_target->hist.depth = 0;
	upgrade_gv_target->alt_hist->depth = 0;
	reorg_gv_target = targ_alloc(MAX_KEY_SZ, NULL, NULL);		/* UPGRADE uses REORG swap functionality */
	reorg_gv_target->hist.depth = 0;
	reorg_gv_target->alt_hist->depth = 0;
	grab_crit(reg, WS_64);
	csa->hold_onto_crit = TRUE;
	new_blk_num = blks_in_way + 2;					/* skip the 1st bml and DIR_ROOT (blocks 0 & 1) */
	for (index = level = 0; (num_blks_mv > index) && (level < MAX_BT_DEPTH); level++)
	{	/* loop through all levels of blocks relocating; to assure finding parents, must move deeper/lower levels 1st */
		if ((NULL == blks_to_mv_levl_ptr[level]) || (0 == blks_to_mv_levl_ptr[level][LEVEL_CNT]))
			continue;		/* no blocks (left) at this level */
		for (old_blk_num = 2; old_blk_num <= (1 + blks_in_way); old_blk_num++)
		{	/* skipping block 1 (DIR_ROOT), loop through blocks of a level relocating to a local bit map */
			if (0 == blks_to_mv_levl_ptr[level][old_blk_num])
				continue;				/* not marked to move while processing this level */
			assert(blks_to_mv_levl_ptr[level][LEVEL_CNT] && blks_to_mv_levl_ptr[level][old_blk_num]);
			blks_to_mv_levl_ptr[level][LEVEL_CNT]--;
			while (FILE_EXTENDED == (new_blk_num = SIMPLE_FIND_FREE_BLK(new_blk_num, FALSE, TRUE)))
			{	/* Extension occurred in MM, this requires a remap. Note that BG won't have the same return */
				assert(is_mm);
				CHECK_MM_DBFILEXT_REMAP_IF_NEEDED(cs_addrs, reg);
				for (gvt = gv_target_list; NULL != gvt; gvt = gvt->next_gvnh)
					gvt->clue.end = 0; /* Invalidate gvt clues due to remap. Should force a fresh search */
				cs_addrs->dir_tree->clue.end = 0;
				new_blk_num = blks_in_way + 2;	/* Reset to lowest block number count */
			}
			if (0 > new_blk_num)
			{	/* extend failed - give up on region; things left in a bad state */
				assert(0 < new_blk_num);
				t_abort(gv_cur_region, cs_addrs);	 		/* do crit and other cleanup */
				mv_blk_err = TRUE;
				return new_blk_num;					/* return reason for extension failure */
			}
			assert(blks_in_way < new_blk_num);
			t_blk_num = new_blk_num--;					/* For use with mu_swap_blk() later */
			blkBase = t_qread(old_blk_num, &blkhist->cycle, &blkhist->cr);
			if (NULL == blkBase)
			{	/* read failed */
				assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
				t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
				mv_blk_err = TRUE;
				util_out_print("Region !AD : Read of block @x!@XQ failed;", FALSE, REG_LEN_STR(reg), &old_blk_num);
				util_out_print(" going on to next block.", TRUE);
				continue;
			}
			blkHdr = *((blk_hdr_ptr_t)blkBase);
			if (SIZEOF(blk_hdr) == blkHdr.bsiz)
			{	/* data level of a KILLed global - dealt with by ditch_dead_globals & upgrade_dir_tree */
				assert(0 == blkHdr.levl);
				t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
				if (NULL == skipped_killed_ptr)
				{	/* allocate pointers for an additional level */
					skipped_killed_ptr = malloc(SIZEOF(int) * 2 * blks_in_way + 1);
					memset(skipped_killed_ptr, 0, SIZEOF(int) * 2 * blks_in_way + 1);
				}
				skipped_killed_ptr[old_blk_num] = index;
				continue;
			}
			blkhist->blk_num = old_blk_num;
			blkhist->buffaddr = blkBase2 = blkBase;
			blkhist->level = blkHdr.levl;
			recBase = blkBase + SIZEOF(blk_hdr);
			status = gen_hist_for_blk(blkhist, blkBase2, recBase, &gvname, gvnh_reg);
			if (cdb_sc_normal != status)
			{	/* skip this block as it should be associated with a KILL'd global */
				if ((cdb_sc_starrecord != status) || (1 != blkHdr.levl))
				{	/* not a KILL'd global artifact */
					mv_blk_err = TRUE;
					util_out_print("Region !AD : Directory lookup failed (!UL), preventing",
							FALSE, REG_LEN_STR(reg), status);
					util_out_print(" move of @x!@XQ; going on to next block.", TRUE, &old_blk_num);
				}
				t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
				if (NULL == skipped_killed_ptr)
				{	/* allocate pointers for an additional level */
					skipped_killed_ptr = malloc(SIZEOF(int) * 2 * blks_in_way + 1);
					memset(skipped_killed_ptr, 0, SIZEOF(int) * 2 * blks_in_way + 1);
				}
				skipped_killed_ptr[old_blk_num] = index;
				continue;
			}
			kill_set_list.used = 0;
			assert(!update_trans && !need_kip_incr);
			update_trans = UPDTRNS_DB_UPDATED_MASK;
			t_begin_crit(ERR_MUNOUPGRD);
			/* At this point "gv_target->hist" contains the history for "old_blk_num" (generated by
			 * the "gen_hist_for_blk()" call above. Now that we started a new transaction by the
			 * "t_begin_crit()" call above, make sure the blocks in the history are added to the
			 * "cw_stagnate" hashtable to ensure the corresponding global buffers do not get reused for
			 * another block as that would cause restarts and we cannot afford those since we are standalone.
			 */
			if (is_bg)
				GVT_HIST_CWS_INSERT(&gv_target->hist);
			if (!(dt = (DIR_ROOT == gv_target->root)))					/* WARNING assignment */
			{	/* get the target global tree */
				memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
				recBase = gv_target->hist.h[level].buffaddr + gv_target->hist.h[level].curr_rec.offset;
				status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, blkHdr.levl,
						&(gv_target->hist.h[level]), recBase);
				if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
				{	/* failed to parse directory tree record for desired key */
					assert(cdb_sc_normal == status);
					t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
					mv_blk_err = TRUE;
					util_out_print("Region !AD : Parse of directory for key !AD failed (!UL)", FALSE,
						REG_LEN_STR(reg), gvname.var_name.len, gvname.var_name.addr, status);
					util_out_print(" preventing move of 0x!@XQ; going on to next block.", TRUE, &old_blk_num);
					continue;
				}
				/*verify expanded key matches the name */
				assert(0 == memcmp(gvname.var_name.addr, gv_currkey->base, gvname.var_name.len));
				gv_target->clue.end = 0;
				if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))		/* WARNING: assignment */
				{	/* failed to get global tree history for key */
					assert(cdb_sc_normal == status);
					t_abort(gv_cur_region, cs_addrs); /* do crit and other cleanup */
					mv_blk_err = TRUE;
					util_out_print("Region !AD : Search for for key !AD failed (!UL), so", FALSE,
						REG_LEN_STR(reg), gvname.var_name.len, gvname.var_name.addr, status);
					util_out_print(" can't move block 0x!@XQ; going on to next block.", TRUE, &old_blk_num);
					continue;
				}
			}
			/* Step 3: Move the block to its new location */
			reorg_gv_target->hist.depth = 0;
			reorg_gv_target->alt_hist->depth = 0;
			if (is_bg)
			{	/* Standalone operation not following normal concurrency patterns */
				if (NULL == (bt = (bt_get(old_blk_num))))
				{	/* Do this to prevent history/buffer reuse induced restart */
					bt_put(reg, old_blk_num);
					bt = bt_get(old_blk_num);
				}
				bt->tn = bt->killtn = start_tn - 1;
			}
			if (dt || (0 == blkHdr.levl) || (old_blk_num != gv_target->root))
			{	/* The block being moved is not a root block */
				assert(dt ? (old_blk_num == gv_target->hist.h[blkHdr.levl].blk_num)
					 : blkHdr.levl != gv_target->hist.depth);
				/* mu_swap_blk increments the destination block hint to find a block to swap into; that logic was
				 * designed for a regular reorg operation and is not relevant to this case, hence the decrement
				 * of new_blk_num location selected a ways above into a "hint" landing us on the desired block
				 */
				mu_reorg_in_swap_blk = TRUE;
				status = mu_swap_blk(blkHdr.levl, &new_blk_num, &kill_set_list, NULL, old_blk_num);
				mu_reorg_in_swap_blk = FALSE;
				if (cdb_sc_normal != status)
				{	/* swap to get the block out of the blks_in_way zone failed */
					assert(cdb_sc_normal == status);
					t_abort(gv_cur_region, cs_addrs);		/* do crit and other cleanup */
					mv_blk_err = TRUE;
					util_out_print("Region !AD : Failed swap of block 0x!@XQ to 0x!@XQ", FALSE,
						REG_LEN_STR(reg), &old_blk_num, new_blk_num);
					util_out_print(" (!UL); moving on to next block.", TRUE, status);
					continue;
				}
				assert((blks_in_way + 1 != new_blk_num) && (t_blk_num == new_blk_num));
				assert(new_blk_num == cw_set[0].blk);
				assert(ROUND_DOWN2(new_blk_num, BLKS_PER_LMAP) == cw_set[2].blk);
				t_blk_num -= blks_in_way;	/* t_blk_num is new block number after offset applied */
				if (dt)
					mu_reorg_process = TRUE;
				ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED);
				if (0 == ret_tn)
				{	/* commit failed */
					assert(ret_tn);
					status = (t_tries) ? t_fail_hist[t_tries - 1] : cdb_sc_committfail;
					t_abort(gv_cur_region, cs_addrs);		/* do crit and other cleanup */
					mu_reorg_process = FALSE;
					mv_blk_err = TRUE;
					util_out_print("Region !AD : Failed commit of block 0x!@XQ (!UL);", FALSE,
						REG_LEN_STR(reg), &old_blk_num, status);
					util_out_print(" swap to 0x!@XQ (0x!@XQ) going on to next block.", TRUE,
							&new_blk_num, &t_blk_num);
					continue;
				}
				assert((1 == kill_set_list.used) && !kill_set_list.blk[0].level
					&& !kill_set_list.blk[0].flag && (old_blk_num == kill_set_list.blk[0].block));
				GVCST_BMP_MARK_FREE(&kill_set_list, ret_tn, inctn_opcode, inctn_bmp_mark_free_mu_reorg,
					inctn_opcode, csa);
				mu_reorg_process = FALSE;
				kill_set_list.used = 0;
				if (0 == ret_tn)
				{	/* bit map commit failed */
					assert(ret_tn);
					status = (t_tries) ? t_fail_hist[t_tries - 1] : cdb_sc_committfail;
					t_abort(gv_cur_region, cs_addrs);		/* do crit and other cleanup */
					mv_blk_err = TRUE;
					util_out_print("Region !AD : Failed to free block 0x!@XQ (!UL);",
							FALSE, REG_LEN_STR(reg), &old_blk_num, status);
					util_out_print(" going on to next block; this only matters", FALSE);
					util_out_print(" if another type of problem stops region conversion.", TRUE);
				}
				if (debug_mupip)
					util_out_print("moved level !UL block 0x!@XQ to 0x!@XQ (0x!@XQ).", TRUE,
							blkHdr.levl, &old_blk_num, &new_blk_num, &t_blk_num);
				blk_moved_cnt++;
			} else
			{	/* The block being moved is a root block */
				t_blk_num -= blks_in_way;
				move_root_block(new_blk_num++, old_blk_num, gvnh_reg, &kill_set_list);
				if (debug_mupip)
					util_out_print("moved level !UL root block 0x!@XQ to 0x!@XQ (0x!@XQ).", TRUE,
							blkHdr.levl, &old_blk_num, &new_blk_num, &t_blk_num);
				root_moved_cnt++;
			}
			index++;
			t_abort(gv_cur_region, cs_addrs);	/* do crit and other cleanup */
		}	/* loop through blocks of a level relocating to a local bit map */
	}	/* loop through all levels of blocks relocating to a local bit map */
	/* completed move of all but the DIR_ROOT */
	for (level = 0; level < MAX_BT_DEPTH; level++)
	{	/* release momory used by the loops above */
		if (NULL != blks_to_mv_levl_ptr[level])
			free(blks_to_mv_levl_ptr[level]);
	}
	if (NULL != skipped_killed_ptr)
		free(skipped_killed_ptr);
	/* Check if there were any errors while moving the blocks */
	if (TRUE == mv_blk_err)
	{	/* one or more errors while moving blocks, so don't process DT or move block 1 (DIR_ROOT) to new location
		 * just print an error and move on to the next region
		 */
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		util_out_print("Region !AD : Not all blocks were moved successfully;", FALSE, REG_LEN_STR(reg));
		util_out_print(" cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	/* identify KILL'd tree stumps before moving DIR_ROOT as some may be stuck in the blks_in_way zone "without history" */
	mu_reorg_more_tries = TRUE;	/* so "mu_upgrade_pin_blkarray[]" will be honored by "db_csh_getn()" */
	mu_upgrade_pin_blkarray_idx = 0;	/* initialize global variable in outermost call to "ditch_dead_globals()" */
	status = ditch_dead_globals(DIR_ROOT, blks_in_way, &child_cr);
	if (is_bg && (NULL != child_cr))
	{	/* Release the cache record, transitively making the corresponding buffer, that this function just
		 * modified, available for re-use. Doing so ensures that all the CRs being touched as part of the
		 * REORG UPGRADE do not accumulate creating a situation where "when everything is special, nothing
		 * is special" resulting in parent blocks being moved around in memory which causes restarts.
		 */
		child_cr->refer = FALSE;
	}
	mu_reorg_more_tries = FALSE;	/* no longer need to honor "mu_upgrade_pin_blkarray[]" in "db_csh_getn()" */
	if (cdb_sc_normal != status)
	{	/* not able to process for dead globals */
		assert(cdb_sc_normal == status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		util_out_print("Region !AD : Directory processing KILL'd globals failed", FALSE, REG_LEN_STR(reg));
		util_out_print(" (!UL). Cannot complete mastermap enlargement.", TRUE, status);
		return ERR_MUNOFINISH;
	}
	tot_kill_block_cnt = 2 * killed_gbl_cnt;					/* 2 = gvt root + 1 empty data block */
	old_blk_num = blkhist->blk_num = DIR_ROOT;
	new_blk_num = blks_in_way + 1;	/* step past the local bit map */
	blkBase = blkhist->buffaddr = t_qread(old_blk_num, &blkhist->cycle, &blkhist->cr);
	if (NULL == blkBase)
	{	/* read failed */
		status = rdfail_detail;
		assert(cdb_sc_normal == status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		util_out_print("Region !AD : Read of block @x!@XQ failed", FALSE, REG_LEN_STR(reg), &old_blk_num);
		util_out_print(" (!UL). Cannot complete mastermap enlargement.", TRUE, status);
		return ERR_MUNOFINISH;
	}
	blkHdr = *((blk_hdr_ptr_t)blkBase);
	blkhist->level = blkHdr.levl;
	recBase = blkBase + SIZEOF(blk_hdr);
	status = gen_hist_for_blk(blkhist, blkBase, recBase, &gvname, gvnh_reg);
	if (cdb_sc_normal != status)
	{	/* failed to develop a history */
		assert(cdb_sc_normal == status);
		t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
		return status;
	}
	blkhist->tn = csd->trans_hist.curr_tn;
	gv_target->clue.end = 0;		/* Invalidate the clue */
	if (is_bg)
	{
		if (NULL == (bt = (bt_get(old_blk_num))))
		{
			bt_put(reg, old_blk_num);	/* unless/until cc differs, prevent hist restart */
			bt = bt_get(old_blk_num);
		}
		bt->tn = bt->killtn = start_tn - 1;
	}
	if (debug_mupip)
		util_out_print("Move lvl:!UL root block 0x!@XQ to 0x!@XQ.", TRUE, blkHdr.levl, &old_blk_num, &new_blk_num);
	memcpy(gv_currkey->base, gvname.var_name.addr, gvname.var_name.len);
	gv_currkey->end = gvname.var_name.len;
	gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
	gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
	gv_altkey = gv_currkey;
	assert(!update_trans && !need_kip_incr);
	update_trans = UPDTRNS_DB_UPDATED_MASK;
	t_begin_crit(ERR_MUNOUPGRD);
	kill_set_list.used = 0;
	/* WARNING assignment below; pre-decrement new_blk_num */
	if (SS_NORMAL != (status = move_root_block(--new_blk_num, old_blk_num, gvnh_reg, &kill_set_list)))
	{	/* relocation of the DIR_ROOT (block 1) failed */
		assert(SS_NORMAL == status);
		t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		util_out_print("Region !AD : Move of directory tree root failed (!UL);", FALSE, REG_LEN_STR(reg), status);
		util_out_print(" cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	root_moved_cnt++;							/* move complete */
	reorg_gv_target->gvname.var_name.len = gv_target->gvname.var_name.len;	/* needed by SAVE_ROOTSRCH_ENTRY_STATE */
	memcpy(reorg_gv_target->gvname.var_name.addr, gv_target->gvname.var_name.addr,
				reorg_gv_target->gvname.var_name.len);
	t_abort(gv_cur_region, cs_addrs); 					/* do crit and other cleanup */
	assert(DIR_ROOT == gv_target->hist.h[blkHdr.levl].blk_num);
	gv_target->hist.h[blkHdr.levl].blk_num += blks_in_way;
	/* Update the header to reflect the new svbn */
	wcs_flu(WCSFLU_NONE);							/* push it all out */
	csd->start_vbn += (blks_in_way * blk_size) / DISK_BLOCK_SIZE;		/* upgraded file has irregular start_vbn */
	csd->fully_upgraded = FALSE;
	csd->db_got_to_v5_once = FALSE;						/* Signal to treat all recycled blocks as free */
	csd->desired_db_format = GDSV6p;					/* now adjust version for upgrade of DT */
	csd->desired_db_format_tn = csd->trans_hist.curr_tn;
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_DBDSRDFMTCHNG, 9, DB_LEN_STR(reg),
		LEN_AND_STR(gtm_dbversion_table[GDSV6p]), LEN_AND_LIT("MUPIP UPGRADE"),
		process_id, process_id, &csd->desired_db_format_tn);
	offset = csd->offset = blks_in_way;					/* offset to pointers in pre-upgrade blks */
	csd->trans_hist.total_blks -= blks_in_way;
	csd->trans_hist.free_blocks -= (blks_in_way - bmls_to_work);		/* lost space less bmls which remain busy */
	reinitialize_hashtab_mname(gd_header->tab_ptr);
	csd->blks_to_upgrd = csd->trans_hist.total_blks - csd->trans_hist.free_blocks
		- DIVIDE_ROUND_UP(csd->trans_hist.total_blks, BLKS_PER_LMAP);	/* Total blocks to upgrade */
	/* Use the following functions to forcefully clear the entire array */
	SYNC_ROOT_CYCLES(csa);
	if (is_bg)
		clear_cache_array(csa, csd, gv_cur_region, (block_id)0, csd->trans_hist.total_blks - blks_in_way);
	else
	{	/* MM needs a fresh start with gv_target caching as it is full of now outdated block_ids and buffaddrs */
		assert(is_mm);
		for (gvt = gv_target_list; NULL != gvt; gvt = gvt->next_gvnh)
		{	/* Iterate over the GV targets, resetting clues and applying offsets to search history */
			gvt->clue.end = 0;
			for (i = MAX_BT_DEPTH; 0 <= i; i--)
			{
				if (DIR_ROOT == gvt->hist.h[i].blk_num)
					continue;
				if (offset > gvt->hist.h[i].blk_num)
				{	/* Can't be offset, so nullify it */
					gvt->hist.depth = i;
					gvt->hist.h[i].blk_num = HIST_TERMINATOR;
					gvt->hist.h[i].buffaddr = NULL;
				} else
					gvt->hist.h[i].blk_num -= offset;
			}
		}
#ifdef _AIX
		wcs_mm_recover(reg, csd->start_vbn - old_vbn);
#endif
	}
	wcs_flu(WCSFLU_IN_COMMIT | WCSFLU_FLUSH_HDR | WCSFLU_CLEAN_DBSYNC);
	wcs_recover(reg);
	SYNC_ROOT_CYCLES(csa);
	if (NULL == (blkBase = t_qread(DIR_ROOT, &blkhist->cycle, &blkhist->cr)))		/* WARNING assignment */
	{	/* read failed */
		assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
		csa->hold_onto_crit = FALSE;
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		util_out_print("Region !AD : Read of block @x!@XQ failed;", FALSE, REG_LEN_STR(reg), &old_blk_num);
		util_out_print(" Cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	csa->dir_tree->hist.depth = 1;	/* at least for MM we need to reestablish the dir_tree before additional searches */
	if (csa->dir_tree->hist.h[0].blk_num > offset)
		csa->dir_tree->hist.h[0].blk_num -= offset;
	csa->dir_tree->hist.h[0].buffaddr = blkBase;
	csa->dir_tree->hist.h->tn = ((blk_hdr_ptr_t)(csa->dir_tree->hist.h[0].buffaddr))->tn - 1;
	if (cdb_sc_normal != (status = adjust_master_map(blks_in_way, reg))) /* Warning assignment */
	{	/* adjust_master_map() prints the relevant error context */
		csa->hold_onto_crit = FALSE;
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		return ERR_MUNOFINISH;
	}
	inctn_opcode = inctn_invalid_op;
	assert((0 == csd->kill_in_prog) && (NULL == kip_csa));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	csd->db_got_to_v5_once = TRUE;	/* Signal that all recycled blocks are now marked free */
	util_out_print("Region !AD : Master map required 0x!@XQ blocks;", FALSE, REG_LEN_STR(reg), &blks_in_way);
	util_out_print(" Size is now at 0x!@XQ blocks after any extension.", TRUE, &csd->trans_hist.total_blks);
	tot_dt = tot_kill_byte_cnt = tot_levl_cnt = tot_splt_cnt = 0;
	mu_reorg_more_tries = TRUE;	/* so "mu_upgrade_pin_blkarray[]" will be honored by "db_csh_getn()" */
	for (i = 1; i >= 0; i--)
	{	/* Upgrade_dir_tree does both the offest adjustment and the pointer enlargement. It is possible
		 * for DIR_ROOT to split, so loop once more to account for it.
		 */
		mu_upgrade_pin_blkarray_idx = 0;	/* initialize global variable in outermost call to "upgrade_dir_tree()" */
		status = upgrade_dir_tree(DIR_ROOT, blks_in_way, reg, &lost, &child_cr);
		if (is_bg && (NULL != child_cr))
		{	/* Release the cache record, transitively making the corresponding buffer, that this function just
			 * modified, available for re-use. Doing so ensures that all the CRs being touched as part of the
			 * REORG UPGRADE do not accumulate creating a situation where "when everything is special, nothing
			 * is special" resulting in parent blocks being moved around in memory which causes restarts.
			 */
			child_cr->refer = FALSE;
		}
		if (cdb_sc_normal <= status)
			break;
	}
	mu_reorg_more_tries = FALSE;	/* no longer need to honor "mu_upgrade_pin_blkarray[]" in "db_csh_getn()" */
	if (cdb_sc_normal != status)
	{	/* directory tree upgrade failed */
		csa->hold_onto_crit = FALSE;
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		util_out_print("Region !AD : Directory tree upgrade failed (!UL);", FALSE, REG_LEN_STR(reg), status);
		util_out_print(" cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	udi = FILE_INFO(reg);	/* Allocate an aligned buffer for new bmm (in case AIO), see DIO_BUFF_EXPAND_IF_NEEDED */
	new_bmm_size = ROUND_UP(MASTER_MAP_SIZE_DFLT, cs_data->blk_size) + OS_PAGE_SIZE;
	bml_buff = malloc(new_bmm_size);
	bmm_base = (sm_uc_ptr_t)ROUND_UP2((sm_long_t)bml_buff, OS_PAGE_SIZE);
	assert(OS_PAGE_SIZE >= (bmm_base - bml_buff));
	memset(bmm_base, BMP_EIGHT_BLKS_FREE, MASTER_MAP_SIZE_DFLT); 		/* Initialize entire bmm to FREE */
	memcpy(bmm_base, csa->bmm, csd->master_map_len);			/* Overlay V6 bmm onto consolidated bmm */
	DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, SGMNT_HDR_LEN, bmm_base,	/* Write new BMM after file header */
			MASTER_MAP_SIZE_DFLT, status);
	assert(SS_NORMAL == status);
	free(bml_buff);								/* A little early in case of err */
	if (SS_NORMAL != status)
	{	/* file header write failed */
		save_errno = errno;
		csa->hold_onto_crit = FALSE;
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(13) ERR_DBFILERR, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("Error initializing additional master bit map space."),
			ERR_TEXT, 2, LEN_AND_STR(STRERROR(save_errno)));
		return ERR_MUNOFINISH;
	}
	csd->master_map_len = MASTER_MAP_SIZE_DFLT;
	csd->certified_for_upgrade_to = GDSV7m;
	assert(csd->blks_to_upgrd == (csd->trans_hist.total_blks - csd->trans_hist.free_blocks - tot_dt
			- DIVIDE_ROUND_UP(csd->trans_hist.total_blks, BLKS_PER_LMAP)));
	csd->tn_upgrd_blks_0 = csd->reorg_upgrd_dwngrd_restart_block = csd->blks_to_upgrd_subzero_error = 0;
	csd->free_space = BLK_ZERO_OFF(csd->start_vbn) - SIZEOF_FILE_HDR(csd);	/* Calculate free space between HDR and start VBN */
	memset(csd->reorg_restart_key, 0, MAX_MIDENT_LEN + 1);
	db_header_dwnconv(csd);					/* revert the file header to V6 format so we can save it */
	db_header_upconv(csd);								/* finish transition to new header */
	/* REORG -UPGRADE is next:
	 * - Set the DB's upgrade status as FALSE
	 * - Set the format change tn as current tn
	 */
	csd->desired_db_format = GDSV7m;			/* Transitional phase 2 - all new blocks are V7m */
	csd->minor_dbver = GDSMVCURR;				/* Raise the DB minor version */
	MEMCPY_LIT(csd->label, GDS_LABEL);			/* Change to V7 label, but not fully upgraded */
	csd->desired_db_format_tn = csd->trans_hist.curr_tn;	/* Phase 1 complete, set the format change TN */
	csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
	INCREMENT_CURR_TN(csd);
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_DBDSRDFMTCHNG, 9, DB_LEN_STR(reg),
		LEN_AND_STR(gtm_dbversion_table[GDSV6p]), LEN_AND_LIT("MUPIP UPGRADE"),
		process_id, process_id, &csd->desired_db_format_tn);
	CHECK_TN(cs_addrs, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
	wcs_flu(WCSFLU_IN_COMMIT | WCSFLU_FLUSH_HDR | WCSFLU_CLEAN_DBSYNC);
	/* Finished region. Cleanup for move onto next region. */
	csa->hold_onto_crit = FALSE;
	rel_crit(reg);
	mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
	util_out_print("Region !AD : MUPIP UPGRADE -MASTERMAP completed.", TRUE, REG_LEN_STR(reg));
	util_out_print("Region !AD : Relocated !UL root blocks and !UL other blocks.",
			TRUE, REG_LEN_STR(reg), root_moved_cnt, blk_moved_cnt);
	util_out_print("Region !AD : Removed !UL KILL'ed globals, freeing !UL blocks and !UL bytes from directory tree.",
			TRUE, REG_LEN_STR(reg), killed_gbl_cnt, tot_kill_block_cnt, tot_kill_byte_cnt);
	util_out_print("Region !AD : Upgraded !UL directory tree blocks, splitting !UL blocks, adding !SL directory tree level!AD.",
			TRUE, REG_LEN_STR(reg), tot_dt, tot_splt_cnt, tot_levl_cnt, 1, 1 == tot_levl_cnt ? " " : "s");
	return SS_NORMAL;
}

/******************************************************************************************
* This interlude function, given a region and extention, invokes gdsfilext
*
* Input Parameters:
*	extension: blocks to extend
*	reg: identification for the region being upgraded
* Output Parameters:
*	(int4)SS_NORMAL or ERR_MUNOFINISH
******************************************************************************************/
int4 upgrade_extend(gtm_int8 extension, gd_region *reg)
{
	int4	status;

	util_out_print("Region !AD : Not enough free blocks to extend the master map & provide additional index blocks.",
		FALSE, REG_LEN_STR(reg));
	if (0 == cs_data->extension_size)
	{	/* no extension size in segment data to work with */
<<<<<<< HEAD
		util_out_print("Region !AD: Extension size not set in database header.", TRUE, REG_LEN_STR(reg));
		util_out_print("Region !AD: Perform a MUPIP EXTEND on this region,", FALSE, REG_LEN_STR(reg));
=======
		util_out_print("!/Region !AD : Extension size not set in database header.", TRUE, REG_LEN_STR(reg));
		util_out_print("Region !AD : Perform a MUPIP EXTEND on this region,", FALSE, REG_LEN_STR(reg));
>>>>>>> fdfdea1e (GT.M V7.1-002)
		util_out_print(" otherwise free at least 0x!@XQ blocks to continue.", TRUE, &extension);
		/* Caller function "mupip_upgrade()" would have set "csd->fully_upgraded" to FALSE just before calling us.
		 * We have not modified the V6 database file at this point and are about to issue an error and abort the
		 * upgrade process. So reset "csd->fully_upgraded" back to what it was at the start of the upgrade.
		 */
		cs_data->fully_upgraded = TRUE;
		return ERR_MUNOFINISH;
	}
	util_out_print(" Attempting a file extension on the database.", TRUE); /* This print follows the entry */
	status = GDSFILEXT(extension, cs_data->trans_hist.total_blks, TRANS_IN_PROG_FALSE);
	if (SS_NORMAL != status)
	{	/* extension failed */
		util_out_print("Region !AD : File extension of 0x!@XQ blocks failed.", TRUE, REG_LEN_STR(reg), &extension);
		util_out_print("Region !AD : REGION NOT UPGRADED.", TRUE, REG_LEN_STR(reg));
		return ERR_MUNOFINISH;
	}
	util_out_print("Region !AD : File extension of 0x!@XQ blocks succeeded.", FALSE, REG_LEN_STR(reg), &extension);
	util_out_print(" DB size temporarily at 0x!@XQ blocks.", TRUE, &cs_data->trans_hist.total_blks);
	CHECK_MM_DBFILEXT_REMAP_IF_NEEDED(cs_addrs, reg);
	return SS_NORMAL;
}

/******************************************************************************************
 * This helper function, given a block, and record attempts to create a name and history
 * structure by finding a name and searching for it; we need this because, unlike normal
 * database operations we start with a block rather than a name/key; the code is similar in
 * approach to that in DSE when the operator initiates an operation identified by a block
 *
 * Input Parameters:
 *	blkBase2 points to a block holding a record (typically the first in the block)
 *	recBase points to record in blkBase2 the caller wishes to follow
 * Output Parameters:
 *	gvname points to a structure for storing the name selected for which to search
 *	blkhist points to a structure containing the history from the search
 *	(enum cdb_sc) returns a code containing cdb_sc_normal or a "retry" code
 ******************************************************************************************/
enum cdb_sc gen_hist_for_blk(srch_blk_status *blkhist, sm_uc_ptr_t blkBase2, sm_uc_ptr_t recBase, mname_entry *gvname,
				gvnh_reg_t *gvnh_reg)
{	/* given a block, find a useful key */
	block_id	blk_temp, curr_blk;
	boolean_t	long_blk_id;
	gv_namehead	*save_targ = NULL;
	ht_ent_mname	*tabent;
	int		curr_level, i, key_cmpc, key_len, rec_sz;
	int4		status;
	mstr 		global_collation_mstr;
	unsigned char	*c, *cp, key_buff[MAX_KEY_SZ + 3];
	unsigned short	rlen, rec_no_coll_sz;
	hash_table_mname *tab_ptr;

	long_blk_id = IS_64_BLK_ID(blkBase2);
	curr_blk = blkhist->blk_num;
	curr_level = blkhist->level;
	do
	{
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, blkhist->level, blkhist, recBase);
		if (cdb_sc_starrecord == status)
		{	/* 1st/only record in block is a *-key - drop a level and repeat the loop */
			assert(blkhist->level);
			READ_BLK_ID(long_blk_id, &blk_temp, SIZEOF(rec_hdr) + recBase + key_len);
			blkBase2 = t_qread(blk_temp, &blkhist->cycle, &blkhist->cr);
			if (NULL == blkBase2)
			{	/* Failed to read the indicated block */
				status = (enum cdb_sc)rdfail_detail;
				assert((cdb_sc_normal == status)
						|| (MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog));
				break;
			}
			long_blk_id = IS_64_BLK_ID(blkBase2);
			if (sizeof(blk_hdr) == ((blk_hdr_ptr_t)blkBase2)->bsiz)	/* BYPASSOK: comparison is unsigned */
				return status;	/* Empty block represents a killed global tree; return cdb_sc_starrecord */
			blkhist->level = ((blk_hdr_ptr_t)blkBase2)->levl;
			blkhist->buffaddr = blkBase2;
			recBase = blkBase2 + SIZEOF(blk_hdr);
			continue;
		} else if (cdb_sc_normal != status)
		{	/* failed to parse record */
			assert((cdb_sc_normal == status)
					|| (MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog));
			break;
		} else
		{	/* block has a potentially useful key in the first record */
			assert((MAX_KEY_SZ >= key_len) && (0 == key_cmpc));
			assert((MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog)
					|| (!memcmp(key_buff, blkBase2 + SIZEOF(blk_hdr) + SIZEOF(rec_hdr), key_len)));
		}
		assert((0 != blkhist->level) || (bstar_rec_size(long_blk_id) != ((blk_hdr_ptr_t)blkBase2)->bsiz)
				|| (MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog));
		/* parse for valid unsubscripted name */
		cp = key_buff;
		c = (u_char *)gvname->var_name.addr;
		i = SIZEOF(mident_fixed);
		assert((blkhist->level) || (VALFIRSTCHAR_WITH_TRIG(*cp)));
		if (HASHT_GBL_CHAR1 == *cp)
		{	/* Leading '#' char */
			*c++ = *cp++;
			i--;
		}
		assert((blkhist->level) || (VALFIRSTCHAR(*cp)));
		/* First character must be valid */
		*c++ = *cp++;
		i--;
		while ((0 <= i) && VALKEY(*cp))
		{	/* Counting down, validate all characters in key name */
			*c++ = *cp++;
			i--;
		}
		if ((0 < i) && (KEY_DELIMITER == *cp))
		{	/* usually KEY_DELIMITER, but not necessarily for an index block */
			assert(KEY_DELIMITER == *cp);
			*c++ = KEY_DELIMITER;
			*c = KEY_DELIMITER;
			gvname->var_name.len = cp - key_buff;
			/* When in a non-level zero block - it isn't possible to determine if this is a directory tree
			 * block or an GVT's index block. Directory tree blocks use partial names whereas GVTs use the
			 * actual names
			 */
			if (0 == ((blk_hdr_ptr_t)blkBase2)->levl)
				break;
			if (0 == blkhist->level)	/* Cannot go any lower, implies the above break did not happen */
				return cdb_sc_badlvl;
		}
		assert(blkhist->level);
		READ_BLK_ID(long_blk_id, &blk_temp, (sm_uc_ptr_t)(blkBase2 + SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) + key_len);
		blkBase2 = t_qread(blk_temp, &blkhist->cycle, &blkhist->cr);
		if (NULL == blkBase2)
		{	/* Failed to read the indicated block. Possible for REORG -UPGRADE while online */
			status = (enum cdb_sc)rdfail_detail;
			assert((cdb_sc_normal == status) || (MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog));
			break;
		}
		long_blk_id = IS_64_BLK_ID(blkBase2);
		blkhist->level = ((blk_hdr_ptr_t)blkBase2)->levl;
		blkhist->buffaddr = blkBase2;
		recBase = blkBase2 + SIZEOF(blk_hdr);
	} while (TRUE);
	if (cdb_sc_normal == status)
	{	/* loop finished cleanly */
		if (NULL != gv_target)
		{	/* Reset gv_target's stale history beyond because some loops end at HIST_TERMINATOR and not depth */
			for (i = MAX_BT_DEPTH; i >= 0 ; i--)
			{
				gv_target->hist.h[i].level = i;
				if (i >= gv_target->hist.depth)
				{
					gv_target->hist.h[i].blk_num = HIST_TERMINATOR;
					gv_target->hist.h[i].buffaddr = NULL;
				}
			}
		}
		memcpy(gv_currkey->base, key_buff, key_len + 1);			/* the + 1 picks up a key_delimiter */
		gv_currkey->end = key_len - 1;
		assert(gv_cur_region->open);
		if (IS_MNAME_HASHT_GBLNAME(gvname->var_name))
		{	/* This code is similar to code in sr_port/mur_forward_play_cur_jrec.c */
			COMPUTE_HASH_MNAME(gvname);
			tab_ptr = gv_cur_region->owning_gd->tab_ptr;
			if (NULL != (tabent = lookup_hashtab_mname(tab_ptr, gvname)))	/* WARNING assignment */
			{
				gvnh_reg = (gvnh_reg_t *)tabent->value;
				assert(NULL != gvnh_reg);
				gv_target = gvnh_reg->gvt;
				assert(gv_cur_region == gvnh_reg->gd_reg); /* this is not portable into a macro */
				gv_cur_region = gvnh_reg->gd_reg;
				assert(gv_cur_region->open);
			} else
			{
				assert(IS_REG_BG_OR_MM(gv_cur_region));
				gv_target = (gv_namehead *)targ_alloc(gv_cur_region->max_key_size, gvname, gv_cur_region);
				GVNH_REG_INIT(gd_header, tab_ptr, NULL, gv_target, gv_cur_region, gvnh_reg, tabent);
			}
			/* if (!TREF(jnl_extract_nocol)) */
				GVCST_ROOT_SEARCH;
		} else
		{
			COMPUTE_HASH_MNAME(gvname);
			GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, gvname, gvnh_reg);
		}
		if (0 == gv_target->root)
		{	/* Something went wrong in the above root search */
			assert(MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog);
			return cdb_sc_gvtrootnonzero;
		}
		t_begin_crit(ERR_MUNOUPGRD);
		if (gv_currkey->end != (key_len - 1))
		{	/* gv_bind_name/gvcst_root_search can overwrite gv_currkey */
			memcpy(gv_currkey->base, key_buff, key_len + 1);	/* the + 1 picks up a key_delimiter */
			gv_currkey->end = key_len - 1;
		}
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
		gv_altkey = gv_currkey;
		gv_target->clue.end = 0;
		if (DIR_ROOT != curr_blk)
		{
			status = gvcst_search(gv_currkey, NULL);
			assert((cdb_sc_normal == status) || (MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog));
			if (cdb_sc_normal != status)
			{
				t_abort(gv_cur_region, cs_addrs);
				return status;
			}
		}
		t_abort(gv_cur_region, cs_addrs);
		if ((curr_blk != gv_target->root)
			&& (curr_blk != gv_target->hist.h[curr_level].blk_num))
		{
			assert(DIR_ROOT != gv_target->root);
			blk_temp = gvnh_reg->gvt->root;
			assert(blk_temp == gv_target->root);
			mu_reorg_in_swap_blk = TRUE;	/* Signal gvcst_root_search to stop in the dir tree, not at the GVT root */
			gv_target->root = 0;
			status = gvcst_root_search(FALSE);
			mu_reorg_in_swap_blk = FALSE;
			/* Clear "cws_reorg_remove_index" (would have been incremented inside "gvcst_root_search()" call above due
			 * to "mu_reorg_in_swap_blk" being TRUE during that call) now that "mu_reorg_in_swap_blk" is also cleared.
			 */
			cws_reorg_remove_index = 0;
			assert(cdb_sc_normal == status);
			if (cdb_sc_normal != status)
				return status;
			assert((DIR_ROOT == upgrade_gv_target->root)
				&& (DIR_ROOT == upgrade_gv_target->hist.h[upgrade_gv_target->hist.depth].blk_num));
			gv_target = upgrade_gv_target;
			gvnh_reg->gvt->root = blk_temp;
			for (i = gv_target->hist.depth; 0 <= i; i--)
				gv_target->hist.h[i].tn = start_tn; /* History should always use the highest TN */
		}
	}
	return status;
}

/******************************************************************************************
 * This interlude function mediates invocation of mu_swap_root for global trees and the
 * directory tree root; mu_swap_root relies on the retry mechanism, and is otherwise opaque
 * with respect to errors, for this use, calls to it currently aways return SS_NORMAL
 *
 * Input Parameters:
 *	new_blk_num specifies a block one less that the desired target location
 *	old_blk_num specifies the block to move
 *	gvnh_reg points to a region structure with hashed names
 *	kill_set_list points to a structure for managing blocks to be freed by the swap
 * Output Parameters:
 *	(int4)SS_NORMAL or ERR_MUNOUPGRD
 ******************************************************************************************/
int4	move_root_block(block_id new_blk_num, block_id old_blk_num, gvnh_reg_t *gvnh_reg, kill_set *kill_set_list)
{
	block_id		did_block;
	glist			root_tag;
	int			root_swap_stat;
	int4			status;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	csd = cs_data;
	root_tag.reg = gv_cur_region;
	root_tag.gvt = gv_target;
	root_tag.gvnh_reg = gvnh_reg;
	t_abort(gv_cur_region, cs_addrs);						/* mu_swap_root[_blk] does its own crit */
	mu_reorg_process = TRUE;
	assert(mu_upgrade_in_prog);
	status = SS_NORMAL;
	memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
	if (DIR_ROOT != old_blk_num)
	{	/* global tree root */
		mu_swap_root(&root_tag, &root_swap_stat, new_blk_num++);
	} else
	{	/* directory tree root */
		assert(0 == csd->kill_in_prog);
		t_begin_crit(ERR_MUNOUPGRD);
		/* Now that we started a new transaction by the "t_begin_crit()" call above, make sure the blocks in
		 * the history that we are passing to "mu_swap_root_blk()" are added to the "cw_stagnate" hashtable to
		 * ensure the corresponding global buffers do not get reused for another block as that would cause
		 * restarts and we cannot afford those since we (i.e. mupip upgrade) are standalone.
		 */
		if (dba_bg == gv_cur_region->dyn.addr->acc_meth)
		{
			GVT_HIST_CWS_INSERT(&gv_target->hist);
			GVT_HIST_CWS_INSERT(gv_target->alt_hist);
		}
		did_block = mu_swap_root_blk(&root_tag, &(gv_target->hist), gv_target->alt_hist, kill_set_list,
				csa->ti->curr_tn, new_blk_num++);
		if (did_block != new_blk_num)
		{	/* swap of DIR_ROOT failed */
			util_out_print("Region !AD : Move of global tree root failed;", FALSE, REG_LEN_STR(gv_cur_region));
			util_out_print(" cannot complete mastermap enlargement.", TRUE);
			assert(did_block == new_blk_num);
			status = ERR_MUNOUPGRD;
		}
		bit_clear((DIR_ROOT - 1) / BLKS_PER_LMAP, MM_ADDR(csd));		/* 3 lines of cleaunup for below hack */
		assert(csa == kip_csa);
		DECR_KIP(csd, csa, kip_csa);
	}
	if (cw_map_depth)
	{	/* the directory/master root requires an extra finagle to make it through t_end */
		assert((DIR_ROOT == old_blk_num) && (2 == cw_map_depth));
		cw_map_depth = 0;
	}
	/* the following is a hack to deal with bitmap maintenance which should be done a better way */
	bm_setmap(ROUND_DOWN(new_blk_num, BLKS_PER_LMAP), new_blk_num, TRUE);
	mu_reorg_process = FALSE;
	return status;
}

/******************************************************************************************
 * This identifies "KILL'd" globals, marks the root block and the empty data block free and
 * points the directory entry to the start_vbn by giving it the value of the offset
 * upgrade_dir_tree then recognizes these entries as disposable and tosses their records
 *
 * Input Parameters:
 *	curr_blk: block_id on which the (recursive) processing works
 * Output Parameters
 * 	*cr points to the cache record that this function was working on for the caller to release
 *	(enum) cdb_sc indicating cdb_sc_normal if all goes well, which we count on
 ******************************************************************************************/
enum cdb_sc ditch_dead_globals(block_id curr_blk, block_id offset, cache_rec_ptr_t *cr)
{
	blk_hdr_ptr_t		rootBase;
	blk_segment		*bs1, *bs_ptr;
	block_id		blk_pter;
	boolean_t		is_bg, long_blk_id;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		child_cr = NULL;
	gvnh_reg_t		*gvnh_reg = NULL;
	int			blk_kill_cnt, blk_seg_cnt, blk_size, blk_sz, key_cmpc, key_len, rec_sz, rec_offset;
	int4			i, status;
	mname_entry		gvname;
	sm_uc_ptr_t		blkBase, blkEnd, recBase;
	srch_blk_status		dirHist, leafHist, rootHist;
	trans_num		ret_tn;
	kill_set		kill_set_list;
	unsigned char		gname[sizeof(mident_fixed) + 2], key_buff[MAX_KEY_SZ + 3];
	unsigned char		*c, *cp;
	DEBUG_ONLY(unsigned int	save_cw_stagnate_count);

	dirHist.blk_num = curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{	/* read failed */
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == (enum cdb_sc)status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		return status;								/* failed to read the indicated block */
	}
<<<<<<< HEAD
	MU_UPGRADE_PIN_BLK(curr_blk, save_cw_stagnate_count);
	if (is_bg = (dba_bg == cs_data->acc_meth))
=======
	if ((is_bg = (dba_bg == cs_data->acc_meth)))
>>>>>>> fdfdea1e (GT.M V7.1-002)
	{
		if (NULL == (bt = (bt_get(curr_blk))))
		{
			bt_put(gv_cur_region, curr_blk);	/* unless/until cc differs, prevent hist restart */
			bt = bt_get(curr_blk);
		}
		bt->tn = bt->killtn = start_tn - 1;
	}
	blkBase = dirHist.buffaddr;
	assert(GDSV6p >= ((blk_hdr_ptr_t)blkBase)->bver);
	blk_size = cs_data->blk_size;
	blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
	blkEnd = blkBase + blk_sz;
	dirHist.level = ((blk_hdr_ptr_t)blkBase)->levl;
	long_blk_id = FALSE;
	if (debug_mupip)
		util_out_print("processing dir tree block 0x!@XQ at level !UL looking for KILL'd globals.", TRUE,
				&curr_blk, dirHist.level);
	if (is_bg)
	{	/* for bg try to hold onto the blk */
		if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(curr_blk))) || (NULL == *cr))
		{
			status = (enum cdb_sc)rdfail_detail;
			assert(cdb_sc_normal == (enum cdb_sc)status);
			t_abort(gv_cur_region, cs_addrs);					/* do crit and other cleanup */
			return status;							/* failed to find the indicated block */
		}
		(*cr)->refer = TRUE;
	}
	/* First get a the key from the first record */
	for (blk_kill_cnt = killed_gbl_cnt, recBase = blkBase + SIZEOF(blk_hdr); recBase < blkEnd; recBase += rec_sz)
	{	/* iterate through a directory tree block */
		if (is_bg && (curr_blk != dirHist.cr->blk))
		{	/* Re-read the original block to ensure against reuse; WARNING assignment below */
			if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
			{	/* read failed */
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				t_abort(gv_cur_region, cs_addrs);		/* do crit and other cleanup */
				return status;					/* Failed to read the indicated block */
			}
			rec_offset = recBase - blkBase;	/* Content change unexpected, reuse current offset */
			blkBase = dirHist.buffaddr;	/* Get new block address and adjust pointers */
			blkEnd = blkBase + blk_sz;
			recBase = blkBase + rec_offset;
			/* for bg try to hold onto the blk */
			if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(curr_blk))) || (NULL == *cr))
			{
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				t_abort(gv_cur_region, cs_addrs);		/* do crit and other cleanup */
				return status;					/* failed to find the indicated block */
			}
			(*cr)->refer = TRUE;
		}	/* else MM does not have this issue in standalone mode */
		assert(recBase != blkBase);
		assert(dirHist.level == ((blk_hdr_ptr_t)blkBase)->levl);
		assert(blk_sz == ((blk_hdr_ptr_t)blkBase)->bsiz);
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, dirHist.level, &dirHist, recBase);
		if (cdb_sc_starrecord == status)
		{	/* Reuse the last key's history to use with *-key as the *-key has none of its own */
			assert((((rec_hdr_ptr_t)recBase)->rsiz + recBase == blkEnd) && (0 != dirHist.level));
			memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
			key_len = 0;
		} else if (cdb_sc_normal != status)
		{	/* failed to parse record */
			assert(cdb_sc_normal == status);
			t_abort(gv_cur_region, cs_addrs);					/* do crit and other cleanup */
			return status;
		}
		GET_BLK_ID_32(blk_pter, recBase + SIZEOF(rec_hdr) + key_len);
		if (0 != dirHist.level)
		{	/* not a level 0 block, so traverse down another level */
			status = ditch_dead_globals(blk_pter, offset, &child_cr);
			if (is_bg && (NULL != child_cr))
			{	/* Release the cache record, transitively making the corresponding buffer, that this function just
				 * modified, available for re-use. Doing so ensures that all the CRs being touched as part of the
				 * REORG UPGRADE do not accumulate creating a situation where "when everything is special, nothing
				 * is special" resulting in parent blocks being moved around in memory which causes restarts.
				 */
				child_cr->refer = FALSE;
			}
			if (cdb_sc_normal != status)
			{	/* recursion failed */
				assert(cdb_sc_normal == status);
				t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
				return status;
			}
		} else
		{	/* Check the global tree to see if this record is for a killed global */
			rootHist.blk_num = blk_pter;
			if (NULL == (rootHist.buffaddr = t_qread(rootHist.blk_num, (sm_int_ptr_t)&rootHist.cycle, &rootHist.cr)))
			{	/* Failed to read the indicated block */
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
				return status;
			}
			rootBase = (blk_hdr_ptr_t)(rootHist.buffaddr);
			if ((1 != rootBase->levl) || ((SIZEOF(blk_hdr) + bstar_rec_size(BLKID_32)) != rootBase->bsiz))
			{
				delete_hashtab_int8(&cw_stagnate, (ublock_id *)&rootHist.blk_num);
				continue;	/* root node not level 1 or isn't just a star record so not a killed global tree */
			}
			GET_BLK_ID_32((leafHist.blk_num), rootHist.buffaddr + SIZEOF(blk_hdr) + SIZEOF(rec_hdr));
			leafHist.buffaddr = t_qread(leafHist.blk_num, (sm_int_ptr_t)&leafHist.cycle, &leafHist.cr);
			if (NULL == leafHist.buffaddr)
			{	/* Failed to read the indicated block */
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
				return status;
			}
			if (SIZEOF(blk_hdr) != ((blk_hdr_ptr_t)leafHist.buffaddr)->bsiz)
			{
				delete_hashtab_int8(&cw_stagnate, (ublock_id *)&rootHist.blk_num);
				delete_hashtab_int8(&cw_stagnate, (ublock_id *)&leafHist.blk_num);
				continue;	/* This leaf node is not empty so this cannot be a killed global tree */
			}
			/* Found a killed global. Generate history for the following bitmap update */
			mu_reorg_process = FALSE;
			gvname.var_name.addr = (char *)gname;
			status = gen_hist_for_blk(&dirHist, blkBase, (blkBase + sizeof(blk_hdr)), &gvname, gvnh_reg);
			mu_reorg_process = TRUE;
			if (cdb_sc_normal != status)
			{	/* failed to develop a history */
				assert(cdb_sc_normal == status);
				t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
				mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
				util_out_print("Region !AD : Block history generation for 0x!@XQ failed (!UL);",
						FALSE, REG_LEN_STR(gv_cur_region), &curr_blk, status);
				util_out_print(" cannot continue with upgrade.", TRUE);
				return status;
			}
			/* Global killed, whack its pointer and free its two blocks */
			/* pointer assigned offset so the offset adjustment makes it 0 as a flag for later handling */
			PUT_BLK_ID_32(recBase + SIZEOF(rec_hdr) + key_len, offset);
			assert(!is_bg || (curr_blk == dirHist.cr->blk));	/* BG ensure we didn't loose the CR */
			assert(!is_bg || (gv_target && (dirHist.cr == gv_target->hist.h[0].cr)));
			inctn_opcode = inctn_bmp_mark_free_mu_reorg;
			assert(!update_trans && !need_kip_incr);
			update_trans = UPDTRNS_DB_UPDATED_MASK;
			t_begin_crit(ERR_MUNOUPGRD);
			CHECK_AND_RESET_UPDATE_ARRAY;					/* reset update_array_ptr to update_array */
			BLK_INIT(bs_ptr, bs1);
			BLK_SEG(bs_ptr, blkBase + SIZEOF(blk_hdr), blk_sz - SIZEOF(blk_hdr));
			if (!BLK_FINI(bs_ptr, bs1))
			{	/* failed to finalize the update */
				status = cdb_sc_mkblk;
				assert(cdb_sc_normal == status);
				t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
				return status;
			}
			t_write(&dirHist, bs1, 0, 0, dirHist.level, TRUE, TRUE, GDS_WRITE_KILLTN);
			inctn_opcode = inctn_mu_reorg;
			if ((trans_num)0 == (ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED))) /* WARNING: assign */
			{	/* failed to commit the update */
				status = t_fail_hist[t_tries - 1];
				assert(cdb_sc_normal == status);
				t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
				return status;
			}
			t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
			assert(blkBase == dirHist.buffaddr);
			assert(blk_sz == ((blk_hdr_ptr_t)blkBase)->bsiz);
			assert(blkEnd == blkBase + blk_sz);
			inctn_opcode = inctn_bmp_mark_free_mu_reorg;
			bm_setmap(ROUND_DOWN2(leafHist.blk_num, BLKS_PER_LMAP), leafHist.blk_num, FALSE);
			bm_setmap(ROUND_DOWN2(rootHist.blk_num, BLKS_PER_LMAP), rootHist.blk_num, FALSE);
			if (debug_mupip)
			{
				util_out_print("deleted KILL'd global name ^!AD with root block 0x!@XQ", FALSE,
						key_len, key_buff, &rootHist.blk_num);
				util_out_print(" and data block 0x!@XQ.", TRUE, &leafHist.blk_num);
			}
			killed_gbl_cnt++;
			delete_hashtab_int8(&cw_stagnate, (ublock_id *)&rootHist.blk_num);
			delete_hashtab_int8(&cw_stagnate, (ublock_id *)&leafHist.blk_num);
		}
	}
	MU_UPGRADE_UNPIN_BLK(curr_blk, save_cw_stagnate_count);
	return cdb_sc_normal;
}

/******************************************************************************************
 * This helper function, recreates the master map to avoid requiring the database move to
 * align with a multiple of eight local bit maps that would permit a memmove to adjust the
 * the master map. The approach reads each local map and set it's master map entry to
 * appropriately correspond, which generates disk traffic, but avoids the big pain of
 * the bit twiddling required to shift the master map by an arbitrary number of bits.
 * This code is similar to a section of code in dse_maps.c; see "Fill in master map"
 *
 * Input Parameters:
 *	None
 * Output Parameters:
 *	(enum cdb_sc) cdb_sc_normal (or hopefully not) a retry code
 ******************************************************************************************/
enum cdb_sc adjust_master_map(block_id blks_in_way, gd_region *reg)
{
	block_id	bml_index, max_bml_chng, total_blks;
	boolean_t	change;
	int4		blks_in_bitmap, firsthint, hint, save_errno, status;
	off_t		offset;
	sgmnt_addrs	*csa;
	sgmnt_data	*csd;
	srch_blk_status	bml_hist;
	unix_db_info	*udi;

	csa = cs_addrs;
	csd = cs_data;
	udi = FILE_INFO(reg);	/* Allocate an aligned buffer for new bmm (in case AIO), see DIO_BUFF_EXPAND_IF_NEEDED */
	total_blks = csa->ti->total_blks;
	max_bml_chng = 0;
	for (bml_index = 0; bml_index < total_blks; bml_index += BLKS_PER_LMAP)
	{	/* (total_blks - bml_index) is used to determine the number of blks in the last lmap of the DB
		 * so the value should never be larger then BLKS_PER_LMAP and thus fit in a int4
		 */
		assert((bml_index + BLKS_PER_LMAP <= total_blks) || (BLKS_PER_LMAP >= (total_blks - bml_index)));
		blks_in_bitmap = (bml_index + BLKS_PER_LMAP <= total_blks) ? BLKS_PER_LMAP : (int4)(total_blks - bml_index);
		assert(1 < blks_in_bitmap);	/* the last valid block in the database should never be a bitmap block */
		if (NULL == (bml_hist.buffaddr = t_qread(bml_index, (sm_int_ptr_t)&bml_hist.cycle, &bml_hist.cr)))
		{	/* Failed to read the indicated block */
			status = (enum cdb_sc)rdfail_detail;
			util_out_print("Region !AD : Read of bitmap block @x!@XQ failed.",
					FALSE, REG_LEN_STR(gv_cur_region), &bml_index);
			util_out_print(" Cannot complete mastermap enlargement.", TRUE);
			assert(cdb_sc_normal == (enum cdb_sc)status);
			return status;
		}
		firsthint = bml_find_free(0, bml_hist.buffaddr + SIZEOF(blk_hdr), blks_in_bitmap);
		if (NO_FREE_SPACE == firsthint)
			change = (0 != bit_clear(bml_index / BLKS_PER_LMAP, csa->bmm));
		else
		{	/* Once the BML has been read, iterate over it, turning RECYCLED to FREE */
			change = (0 == bit_set(bml_index / BLKS_PER_LMAP, csa->bmm));
			/* Iterate with bml_find_free, to mark all non-Busy (not 00) as Free (01) */
			hint = firsthint;
			do
			{
				bml_free(hint, bml_hist.buffaddr + sizeof(blk_hdr));	/* Free "last" hint */
				/* Advance the hint to start at the next position. Two things:
				 * 1 - bml_find_free() needs the hint to be within BLKS_PER_LMAP
				 * 2 - There is no reason for this search to wrap itself since it
				 *	started from the first possible free block. As such, BREAK
				 */
				if (BLKS_PER_LMAP <= hint++)
					break;
				hint = bml_find_free(hint, bml_hist.buffaddr + sizeof(blk_hdr), blks_in_bitmap);
				/* If bml_find_free() wraps, it ends up at firsthint and the loop terminates */
			} while (hint != firsthint);
			offset = BLK_ZERO_OFF(csd->start_vbn) + (off_t)bml_index * csd->blk_size;
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, offset, bml_hist.buffaddr, csd->blk_size, save_errno);
			if (0 != save_errno)	/* Unable to flush the BML, bail */
			{
				util_out_print("Region !AD : Write of bitmap block @x!@XQ failed.",
						FALSE, REG_LEN_STR(gv_cur_region), &bml_index);
				util_out_print(" Cannot complete mastermap enlargement.", TRUE);
				return cdb_sc_blockflush;
			}
		}
		if (change)
			max_bml_chng = bml_index;
	}
	for (total_blks += blks_in_way; bml_index < total_blks; bml_index += BLKS_PER_LMAP)
	{	/* When UPGRADE extends the DB due to insufficient free blocks to cover "blks_in_way", there is a problem
		 * where the above loop does not cover bmls at the end of the DB. These bmls are technically unallocated, BUT
		 * they (possibly) are marked as busy from the prior state. This loop ensures that those bmls are marked free
		 */
		if (0 != bit_set(bml_index / BLKS_PER_LMAP, csa->bmm))
			max_bml_chng = bml_index;
	}
	assert((max_bml_chng <= total_blks) && (0 <= max_bml_chng));
	csa->nl->highest_lbm_blk_changed = max_bml_chng; /* Max possible blkno flushes the entire V6 BMM w/header */
	return cdb_sc_normal;
}

/******************************************************************************************
 * This recursively traverses the directory tree and upgrades the pointers, including those
 * in level 0 blocks which hold data except in the directory tree; this approach is key to
 * addressing the issue of recognizing those level 0 blocks that hold pointers so dsk_read
 * does not have to expend resources doing so; it also deals with KILL'd global vestiges
 * by eliminating their entries in the directory tree while this code has sole control of
 * the database file; as mentioned earlier, it upgrades all the pointers from 32-bit to 64-
 * bit format, which likely entails block splitting; that part is unlikely to be needed in
 * the event of any subsequent master map extentions
 *
 * Input Parameters:
 * 	curr_blk identifies a block in the directory tree to search
 * 	offset specifies the offset to apply to the block pointers due to the shift in
 * 		start_vbn
 * 	blk_size gives the block size for the region
 * 	cs_addrs points to sgmnt_addrs for the region
 * 	gv_cur_region points to the base structure for the region
 * Output Parameters:
 * 	*cr points to the cache record that this function was working on for the caller to release
 * 	(enum_cdb_sc) returns cdb_sc_normal which the code expects or a retry code
 ******************************************************************************************/
enum cdb_sc upgrade_dir_tree(block_id curr_blk, block_id offset, gd_region *reg, block_id_32 *lost, cache_rec_ptr_t *cr)
{
	blk_hdr_ptr_t		rootBase;
	blk_segment		*bs1, *bs_ptr;
	block_id		blk_pter, blk_pter_dbg;
	boolean_t		complete_merge, first_rec, long_blk_id, is_bg, starkey;
	cache_rec		dummy_dt_cr;
	cache_rec_ptr_t		child_cr = NULL;
	enum db_ver		blk_ver;
	gvnh_reg_t		*gvnh_reg = NULL;
	int			adjust, blk_seg_cnt, blk_sz, count, level, max_fill, new_blk_sz, split_blks_added,
				split_levels_added, key_cmpc, key_cmpc_sib, key_len, key_len_sib, rec_offset, rec_sz, rec_sz_sib,
				space_need, v7_rec_sz, max_rightblk_lvl;
	int4			blk_size, status;
	kill_set		kill_set_list;
	mname_entry		gvname;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blkBase, blkEnd, recBase, recBaseN, recBaseP, v7bp, v7recBase, v7end;
	srch_blk_status		dirHist, leafHist, rootHist;
	trans_num		ret_tn;
	unsigned char		gname[sizeof(mident_fixed) + 2], key_buff[MAX_KEY_SZ + 3],
				key_buffN[MAX_KEY_SZ + 3], key_buffP[MAX_KEY_SZ + 3];
	DEBUG_ONLY(unsigned int	save_cw_stagnate_count);

	csa = cs_addrs;
	csd = cs_data;
	dirHist.blk_num = curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{	/* Failed to read the indicated block */
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == (enum cdb_sc)status);
		t_abort(gv_cur_region, csa);							/* do crit and other cleanup */
		return status;
	}
	blkBase = dirHist.buffaddr;
	long_blk_id = IS_64_BLK_ID(blkBase);
	if (long_blk_id)
		return cdb_sc_normal;					/* no need to reprocess an already upgraded level 0 block */
	MU_UPGRADE_PIN_BLK(curr_blk, save_cw_stagnate_count);

	is_bg = (dba_bg == reg->dyn.addr->acc_meth);
	blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
	blkEnd = blkBase + blk_sz;
	dirHist.level = level = ((blk_hdr_ptr_t)blkBase)->levl;
	blk_ver = ((blk_hdr_ptr_t)blkBase)->bver;
	gvname.var_name.addr = (char *)gname;
	if (debug_mupip)
		util_out_print("starting upgrade of directory block 0x!@XQ with version !UL.", TRUE, &curr_blk, (char)blk_ver);
	/* the directory tree needs its index pointers upgraded to V7 format */
	/* First get a the key from the first record. This will be used to get a search history for the block */
	/* Second check how much space is need to upgrade the block and whether the block will need to be split */
	recBaseP = recBase = blkBase + SIZEOF(blk_hdr);
	for (count = 0, first_rec = TRUE; recBase < blkEnd; )
	{	/* iterate through block updating level 0 pointers, removing dead records and counting live records */
		assert((dirHist.buffaddr == blkBase) && (dirHist.level == level));
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase);
		if ((starkey = (cdb_sc_starrecord == status)))					/* WARNING assignment */
		{	/* Reuse the last key's history to use with *-key as the *-key has none of its own */
			assert((((rec_hdr_ptr_t)recBase)->rsiz + recBase == blkEnd) && (0 != level) && (0 == key_len));
			memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
		} else if (cdb_sc_normal != status)
		{	/* failed to parse record */
			assert(cdb_sc_normal == status);
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			return status;
		}
		if ((recBase + rec_sz) > blkEnd)
		{	/* database damage as concurrency conflict should not be possible */
			assert((recBase + rec_sz) <= blkEnd);
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			util_out_print("Region !AD : Failed to process directory tree block 0x!@XQ.",
					TRUE, REG_LEN_STR(reg), &curr_blk);
			return cdb_sc_badoffset;
		}
		GET_BLK_ID_32(blk_pter, recBase + SIZEOF(rec_hdr) + key_len);
		if ((0 == level) && (GDSV5 == blk_ver) && (offset <= blk_pter))
		{	/* dsk_read doesn't adjust data blocks - do it here; like blk_ptr_adjust, but necessarily less efficient */
			if (debug_mupip)
				util_out_print("adjusting directory block 0x!@XQ with version !UL pointer 0x!@XQ", FALSE,
						&curr_blk, (char)blk_ver, &blk_pter);
			assert(FALSE == IS_64_BLK_ID(blkBase));
			assert(offset <= blk_pter);
			blk_pter -= offset;
			if (debug_mupip)
				util_out_print(" to 0x!@XQ.", TRUE, &blk_pter);
			PUT_BLK_ID_32(recBase + SIZEOF(rec_hdr) + key_len, blk_pter);
			assert(0 <= (gtm_int8)blk_pter);
		}
#ifdef DEBUG
		else if (GDSV6p == blk_ver)
		{
			if (debug_mupip)
<<<<<<< HEAD
				util_out_print("reprocessing directory block 0x!@XQ with version !UL pointer 0x!@XQ", TRUE,
=======
				util_out_print("reproccessing directory block 0x!@XQ with version !UL pointer 0x!@XQ.", TRUE,
>>>>>>> fdfdea1e (GT.M V7.1-002)
						&curr_blk, (char)blk_ver, &blk_pter);
		} else
		{
			assert(0 < level);
			if (debug_mupip)
				util_out_print("directory block 0x!@XQ with version !UL pointer 0x!@XQ upgraded on read.", TRUE,
						&curr_blk, (char)blk_ver, &blk_pter);
		}
#endif
		assert((csa->ti->total_blks > blk_pter) && (DIR_ROOT != blk_pter) && (0 <= blk_pter));
		*lost = 0;
		if (level)
		{	/* recurse if it's an index block */
			assert(blk_pter);
			if (is_bg)
			{	/* for bg try to hold onto the blk */
				if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(curr_blk)))	/* WARNING assignment */
						|| (NULL == *cr))
				{
					status = (enum cdb_sc)rdfail_detail;
					assert(cdb_sc_normal == (enum cdb_sc)status);
					t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
					return status;					/* Failed to find the indicated block */
				}
				(*cr)->refer = TRUE;
			}
			status = upgrade_dir_tree(blk_pter, offset, reg, lost, &child_cr);
			if (is_bg && (NULL != child_cr))
			{	/* Release the cache record, transitively making the corresponding buffer, that this function just
				 * modified, available for re-use. Doing so ensures that all the CRs being touched as part of the
				 * REORG UPGRADE do not accumulate creating a situation where "when everything is special, nothing
				 * is special" resulting in parent blocks being moved around in memory which causes restarts.
				 */
				child_cr->refer = FALSE;
			}
			if (0 > status)
			{	/* negative status means a split, except for DIR_ROOT return to caller for reprocessing */
				assert(!*lost);
				if (DIR_ROOT != curr_blk)
				{
					MU_UPGRADE_UNPIN_BLK(curr_blk, save_cw_stagnate_count);
					return status;
				}
				/* DIR_ROOT resets to starting conditions to reprocess tree split */
				assert(blkBase == dirHist.buffaddr);
				blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
				blkEnd = blkBase + blk_sz;
				dirHist.level = level = ((blk_hdr_ptr_t)blkBase)->levl;
				recBaseP = recBase = blkBase + SIZEOF(blk_hdr);
				first_rec = TRUE;
				count = 0;
				rec_sz = ((rec_hdr_ptr_t)(recBase))->rsiz;
				if (debug_mupip)
				{
					util_out_print("reprocessing level !UL directory block 0x!@XQ", FALSE, level, &curr_blk);
					util_out_print(" due to split of child block 0x!@XQ (!UL).", TRUE, &blk_pter, status);
				}
				if (!is_bg || (dirHist.blk_num == dirHist.cr->blk))
					continue;	/* Cannot continue if DIR_ROOT's CR was stomped on */
			} else if (debug_mupip)
				util_out_print("processed level !UL directory block 0x!@XQ pointer 0x!@XQ (!UL).",
						TRUE, level, &curr_blk, &blk_pter, status);
			if (is_bg && (dirHist.blk_num != dirHist.cr->blk))
			{	/* Lost the CR, re-read the original block */
				dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr);
				if (NULL == dirHist.buffaddr)
				{	/* read failed */
					status = (enum cdb_sc)rdfail_detail;
					assert(cdb_sc_normal == (enum cdb_sc)status);
					t_abort(gv_cur_region, cs_addrs);		/* do crit and other cleanup */
					return status;					/* Failed to read the indicated block */
				}
				/* ASSUMPTION: Except for DIR_ROOT, entering this block means that the buffer got lost without a
				 * split. If the block is DIR_ROOT, the math below works because DIR_ROOT's special handling above
				 * resets to starting conditions.
				 * Use the existing recBase offset off of blkBase to restart from the same record in the newly
				 * acquired buffer.
				 */
				rec_offset = recBase - blkBase;
				/* Reset all pointers and proceed */
				blkBase = dirHist.buffaddr;
				blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
				blkEnd = blkBase + blk_sz;
				/* Without a split, size and level remain unchanged */
				assert(first_rec || (blkEnd == (blkBase + blk_sz)));
				assert(first_rec || (dirHist.level == ((blk_hdr_ptr_t)blkBase)->levl));
				assert(recBase < blkEnd);
				dirHist.level = level = ((blk_hdr_ptr_t)blkBase)->levl;
				assert(blk_ver == ((blk_hdr_ptr_t)blkBase)->bver);
				assert(long_blk_id == IS_64_BLK_ID(blkBase));
				recBaseP = recBase = blkBase + rec_offset;
				rec_sz = ((rec_hdr_ptr_t)(recBase))->rsiz;
				/* for bg try to hold onto the blk */
				if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(curr_blk))) || (NULL == *cr))
				{
					status = (enum cdb_sc)rdfail_detail;
					assert(cdb_sc_normal == (enum cdb_sc)status);
					t_abort(gv_cur_region, cs_addrs);		/* do crit and other cleanup */
					return status;					/* failed to find the indicated block */
				}
				(*cr)->refer = TRUE;
			}	/* else MM does not have this issue in standalone mode */
			assert(dirHist.level == ((blk_hdr_ptr_t)blkBase)->levl);
		}
		assert((0 == *lost) || (*lost == blk_pter));
		if ((0 != *lost) || (0 == blk_pter))
		{	/* pointer for a KILL'd global name or empty dt block; next record may need compression count revision */
			if (debug_mupip)
			{
				util_out_print("deleted KILL'd global name or prefix ^!AD with root", FALSE,
						key_len + key_cmpc, key_buff);
				util_out_print(" !AD block 0x!@XQ.", TRUE,
						blk_pter ? 0 : STR_LIT_LEN("pointer in"), blk_pter ? "" : "pointer in",
						blk_pter ? &blk_pter : &curr_blk);
			}
			assert(!starkey || (level && ((recBase + rec_sz) == blkEnd)));
			*lost = 0;
			/* There are 3 records of concern here:
			 * recBaseP	- pointer to record prior to the one to be deleted. There is a placeholder only key_buffP
			 * 		  used because read_record() needs buffer backing. We don't use it, because it is used
			 * 		  to find the pointer to turn the record into a *-key
			 * recBase	- the active record to be deleted. The current key is in key_buff
			 * recBaseN	- pointer the "next" record. This record is backed by key_buffN, which is also unused
			 * 		  because the subsequent copy starts at the next key
			 */
			if ((recBaseN = recBase + rec_sz) != blkEnd)			/* WARNING assignment */
			{	/* get the next record */
				assert(cdb_sc_normal == status);
				DEBUG_ONLY(memcpy(key_buffN, key_buff, key_len + key_cmpc));
				status = read_record(&rec_sz_sib, &key_cmpc_sib, &key_len_sib, key_buffN, level, &dirHist,
					recBaseN);
				if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
				{	/* failed to parse record */
					assert((cdb_sc_normal == status) || (cdb_sc_starrecord == status));
					t_abort(gv_cur_region, csa);				/* do crit and other cleanup */
					return status;
				}
				adjust = key_cmpc_sib - key_cmpc;
			} else
			{	/* no following record - set up to drop this one and leave the loop */
				if (0 == level)
				{	/* "data" level of dir tree has pointers to global tree roots but no *-key */
					adjust = key_cmpc_sib = -rec_sz;
					key_cmpc = rec_sz = 0;
				} else
				{	/* make prior record the star record and adjust the block size */
					assert((starkey) && (BSTAR_REC_SIZE_32 == rec_sz));
					rec_sz = 0;
					status = read_record(&rec_sz_sib, &key_cmpc_sib, &key_len_sib, key_buffP, level,
						&dirHist, recBaseP);
					if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
					{	/* failed to parse record */
						assert((cdb_sc_normal == status) || (cdb_sc_starrecord == status));
						t_abort(gv_cur_region, csa);			/* do crit and other cleanup */
						return status;
					}
					adjust = key_cmpc_sib = -rec_sz_sib;
					recBase = recBaseP;
					GET_BLK_ID_32(blk_pter, recBase + rec_sz_sib - SIZEOF(block_id_32));
					PUT_BLK_ID_32(recBase + SIZEOF(rec_hdr), blk_pter);
					((rec_hdr_ptr_t)(recBase))->cmpc = (unsigned char)0;
					((rec_hdr_ptr_t)(recBase))->rsiz = BSTAR_REC_SIZE_32;
				}
			}
			if (first_rec)
			{	/* it's the first record */
				assert((blkBase + SIZEOF(blk_hdr)) == recBaseP);
				if (0 == rec_sz)
				{	/* this is the only remaining record - make the block empty */
					blkEnd = blkBase + blk_sz;
					*lost = curr_blk;
				}
				assert((0 == key_cmpc) && (adjust == key_cmpc_sib));
			} else
			{
				assert(FALSE == first_rec);
				if (rec_sz)
				{	/* if no following record rec_sz became 0 above */
					assert(!starkey);
					if (key_cmpc > key_cmpc_sib)
					{	/* must pick up compressed prefix from key_buffN, the prior record */
						assert(0 > adjust);
						key_cmpc += adjust;
						assert(key_cmpc == key_cmpc_sib);
						adjust = 0;
					} /* else adjust is positive and pushes the copy adjust bytes away from the
					   * record header below */
				}
			}
			if (rec_sz)
			{	/* leave adjust chars of rec & copy rest of blk; original key_cmpc is valid */
				memmove(recBase + SIZEOF(rec_hdr) + adjust, recBaseN + SIZEOF(rec_hdr),
					blkEnd - (recBaseN + SIZEOF(rec_hdr)));
				((rec_hdr_ptr_t)(recBase))->rsiz = rec_sz_sib + adjust;
				((rec_hdr_ptr_t)(recBase))->cmpc = (unsigned char)key_cmpc;
				adjust -= rec_sz;
			}
			assert((0 >= adjust) && ((rec_sz ? rec_sz : ((blk_hdr_ptr_t)blkBase)->bsiz) >= -adjust));
			blk_sz += adjust;	/* over written sib rec_rec_hdr offsets sz of original rec_hdr */
			blkEnd += adjust;
			tot_kill_byte_cnt -= adjust;
			assert((blkBase + blk_sz) <= blkEnd);
			((blk_hdr_ptr_t)blkBase)->bsiz = (blkEnd - blkBase);
			if (debug_mupip)
			{
				util_out_print("level !UL directory block 0x!@XQ", FALSE, level, &curr_blk);
				util_out_print(" lost record !UL associated with KILL'd global ^!AD.", TRUE,
						count + 1, key_len + key_cmpc, key_buff);
			}
			if (0 == rec_sz)
			{	/* removing last record, so done */
				DEBUG_ONLY(recBase += starkey ? BSTAR_REC_SIZE_32 : ((rec_hdr_ptr_t)(recBase))->rsiz);
				break;
			}
			/* Since the current record is being deleted, "recBaseP" (which should be one record before
			 * recBase when the next iteration starts) should be what it was at the start of this iteration
			 * and so leave it untouched.
			 */
			continue;
		}										/* done with KILL'd global name */
		first_rec = FALSE;
		recBaseP = recBase;
		recBase += rec_sz;
		count++;
	}											/* done preview of records loop */
	if (0 >= count)
	{	/* block became empty and can be ditched */
		mu_reorg_process = TRUE;
		*lost = curr_blk;
		tot_kill_block_cnt++;
		t_abort(gv_cur_region, csa);							/* do crit and other cleanup */
		kill_set_list.used = 1;
		kill_set_list.blk[0].flag = 0;
		kill_set_list.blk[0].block = curr_blk;
		inctn_opcode = inctn_mu_reorg;
		assert(!update_trans && !need_kip_incr);
		GVCST_BMP_MARK_FREE(&kill_set_list, ret_tn, inctn_mu_reorg, inctn_bmp_mark_free_mu_reorg, inctn_opcode, csa)
		kill_set_list.used = 0;
		if (0 == ret_tn)
		{	/* failed to commit bit map update */
			assert(ret_tn);
			util_out_print("Region !AD : Failed to free block 0x!@XQ;", FALSE, REG_LEN_STR(reg), &curr_blk);
			util_out_print(" likely to leave an incorrectly maked busy.", TRUE);
		}
		DECR_BLKS_TO_UPGRD(csa, csd, 1);
		mu_reorg_process = FALSE;
		t_abort(gv_cur_region, csa);							/* do crit and other cleanup */
		if (DIR_ROOT == curr_blk)
		{	/* Database is empty - reinitialize DIR_ROOT and its child as if newly created */
			wcs_recover(reg);
			csd->desired_db_format = GDSV7m;
			mucblkini(GDSV7m);			/* Recreate the DB with V7m directory tree */
			csd->fully_upgraded = TRUE;		/* Since it is V7 */
			MEMCPY_LIT(csd->label, GDS_LABEL);	/* Change to V7 label, fully upgraded */
			csd->minor_dbver = GDSMVCURR;		/* Raise the DB minor version to current */
			/* At this point the file header claims all V6ish settings such that there is a V6p database
			 * certified for upgrade to V7m in spite of there being no actual datablocks. Emit a warning
			 * about the wastage of space for no good reason
			 */
			util_out_print("Region !AD : WARNING, region is effectively empty. MUPIP UPGRADE will adjust the region.",
					TRUE, REG_LEN_STR(reg));
			util_out_print("Region !AD : Please considering recreating the region with V7 for optimal results.",
					TRUE, REG_LEN_STR(reg));
			csd->trans_hist.free_blocks -= 2;
			csd->blks_to_upgrd = 0;
			tot_dt = 2;
			tot_levl_cnt = 1 - level;
		}
		if (debug_mupip)
<<<<<<< HEAD
			util_out_print("dropping level !UL directory block @x!@XQ", TRUE, level, &curr_blk);
		MU_UPGRADE_UNPIN_BLK(curr_blk, save_cw_stagnate_count);
=======
			util_out_print("dropping level !UL directory block @x!@XQ.", TRUE, level, &curr_blk);
>>>>>>> fdfdea1e (GT.M V7.1-002)
		return cdb_sc_normal;
	}
	assert((dirHist.buffaddr == blkBase) && (dirHist.level == level));
	assert(recBase >= blkEnd);
	recBase = blkBase + SIZEOF(blk_hdr);
	assert(is_bg || (NULL == *cr));
	if (0 == *lost)
	{	/* Using the first record, generate a history for this block */
		status = gen_hist_for_blk(&dirHist, blkBase, recBase, &gvname, gvnh_reg);
		if (cdb_sc_normal != status)
		{	/* failed to develop a history */
			assert(cdb_sc_normal == status);
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			return status;
		}
		dirHist = gv_target->hist.h[level];
	}
	((blk_hdr_ptr_t)blkBase)->bver = GDSV6p;	/* Offsets applied to in-memory block, update block header to reflect it */
	if (!is_bg)
	{	/* no cr in mm, but t_write must apply ondsk_blkver to curr_blk, specifically in any split */
		gv_target->hist.h[level].cr = &dummy_dt_cr;
		assert(NULL == dirHist.cr);
		dirHist.cr = &dummy_dt_cr;
		dirHist.cr->blk = curr_blk;
	}
	dirHist.cr->ondsk_blkver = GDSV6p;		/* Update CR (dummy too) because the offsets were applied */
	blk_size = cs_data->blk_size;
	assert((dirHist.buffaddr == blkBase) && (dirHist.level == level) && (dirHist.cr->blk == curr_blk));
	assert(!update_trans && !need_kip_incr);
	update_trans = UPDTRNS_DB_UPDATED_MASK;
	t_begin_crit(ERR_MUNOUPGRD);
	/* Increment existing block size by the record count by rec pointer size difference */
	new_blk_sz = blk_sz + count * (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
	while (0 < (space_need = new_blk_sz - blk_size))						/* WARNING assignment */
	{	/* Insufficient room; WARNING: using a loop construct to enable an alternate pathway out of this level */
		max_fill = (new_blk_sz >> 1);
		if (debug_mupip)
			util_out_print("splitting level !UL dir block @x!@XQ (!UL/!UL).", TRUE, level, &curr_blk, blk_sz, max_fill);
		split_blks_added = split_levels_added = 0;
		mu_reorg_process = TRUE;
		status = mu_split(level, max_fill, max_fill, &split_blks_added, &split_levels_added, &max_rightblk_lvl);
		if (cdb_sc_normal != status)
		{	/* split failed; WARNING: assignment above */
			assert(cdb_sc_normal == status);
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			return status;
		}
		if ((trans_num)0 == (ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED)))
		{	/* failed to commit split */
			status = t_fail_hist[t_tries - 1];
			assert(cdb_sc_normal == status);
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			return status;
		}
		mu_reorg_process = FALSE;
		t_abort(gv_cur_region, csa);							/* do crit and other cleanup */
		blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
		assert((blk_sz <= blk_size) && split_blks_added);
		tot_splt_cnt += split_blks_added;
		tot_levl_cnt += split_levels_added;
		split_blks_added -= split_levels_added;
		MU_UPGRADE_UNPIN_BLK(curr_blk, save_cw_stagnate_count);
		if (0 < split_blks_added)
			return -split_blks_added;						/* force reprocessing after split */
		if (0 < split_levels_added)
			return -split_levels_added;						/* force reprocessing after split */
		assert(FALSE);
	}
	assert((dirHist.buffaddr == blkBase) && (dirHist.level == level));
	assert(new_blk_sz <= blk_size);
	/* Finally upgrade the block */
	assert(dirHist.blk_num == curr_blk);
	assert(GDSV7m != blk_ver);
	dirHist.cr->ondsk_blkver = GDSV7m;							/* maintain as needed */
	assert((dirHist.buffaddr == blkBase) && (dirHist.level == level) && (!is_bg || (dirHist.cr->blk == curr_blk)));
	CHECK_AND_RESET_UPDATE_ARRAY;
	BLK_INIT(bs_ptr, bs1);
	BLK_ADDR(v7bp, new_blk_sz, unsigned char);
	v7recBase = v7bp + SIZEOF(blk_hdr);
	recBase = blkBase + SIZEOF(blk_hdr);
	v7end = v7bp + new_blk_sz;
	for (rec_sz = v7_rec_sz = 0; recBase < blkEnd; recBase += rec_sz, v7recBase += v7_rec_sz)
	{	/* Update the recBase and v7recBase pointers to point to the next record */
		/* Parse the record to account for possible collation information after block pointer */
		/* Because blocks, including level 0, have pointers rather than application data, no worry of spanning or bsiz */
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase);
		if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
		{	/* failed to parse record */
			assert((cdb_sc_normal == status) || (cdb_sc_starrecord == status));
			t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
			return status;
		}
		if ((recBase + rec_sz) > blkEnd)
		{	/* database damage as concurrency conflict should not be possible */
			assert((recBase + rec_sz) <= blkEnd);
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			util_out_print("Region !AD : Failed to process directory tree block 0x!@XQ.",
					TRUE, REG_LEN_STR(reg), &curr_blk);
			return cdb_sc_badoffset;
		}
		GET_BLK_ID_32(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));
		assert((csa->ti->total_blks > blk_pter) && (0 < blk_pter));
		v7_rec_sz = rec_sz + (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
		assert(blk_size > v7_rec_sz);
		/* Push the revised record into the update array */
		if (v7end < (v7recBase + v7_rec_sz))
		{	/* Block size was already calculated, should not get here unless the block was concurrently modified */
			assert(v7end >= (v7recBase + v7_rec_sz));
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			util_out_print("Region !AD : Failed to process directory tree block 0x!@XQ.",
					TRUE, REG_LEN_STR(reg), &curr_blk);
			return cdb_sc_badoffset;
		}
		memcpy(v7recBase, recBase, SIZEOF(rec_hdr) + key_len);
		assert((unsigned short)v7_rec_sz == v7_rec_sz);
		((rec_hdr_ptr_t)v7recBase)->rsiz = (unsigned short)v7_rec_sz;
		PUT_BLK_ID_64((v7recBase + SIZEOF(rec_hdr) + key_len), blk_pter);
#ifdef DEBUG
		GET_BLK_ID_64(blk_pter_dbg, (v7recBase + SIZEOF(rec_hdr) + key_len));
		assert((blk_pter_dbg == blk_pter) && (0 < blk_pter_dbg));
#endif
		if (rec_sz > (SIZEOF(rec_hdr) + key_len + SIZEOF_BLK_ID(BLKID_32)))
		{	/* This record contains collation information that also has to be copied */
			assert(0 == level);
			memcpy(v7recBase + SIZEOF(rec_hdr) + key_len + SIZEOF_BLK_ID(BLKID_64),
				recBase + SIZEOF(rec_hdr) + key_len + SIZEOF_BLK_ID(BLKID_32), COLL_SPEC_LEN);
		}
	}
	BLK_SEG(bs_ptr, v7bp + SIZEOF(blk_hdr), new_blk_sz - SIZEOF(blk_hdr));
	assert(blk_seg_cnt == new_blk_sz);
	if (!BLK_FINI(bs_ptr, bs1))
	{	/* failed to finalize the update */
		status = cdb_sc_mkblk;
		assert(cdb_sc_normal == status);
		t_abort(gv_cur_region, csa);							/* do crit and other cleanup */
		return status;
	}
	t_write(&dirHist, bs1, 0, 0, level, TRUE, TRUE, GDS_WRITE_KILLTN);
	inctn_opcode = inctn_mu_reorg;
	mu_reorg_process = TRUE;
	ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED);
	mu_reorg_process = FALSE;
	MU_UPGRADE_UNPIN_BLK(curr_blk, save_cw_stagnate_count);
	t_abort(gv_cur_region, csa);								/* do crit and other cleanup */
	assertpro(0 != ret_tn);							/* this is a fine fix you've gotten us into Ollie */
	tot_dt++;
#	ifdef DEBUG
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{	/* Failed to read the indicated block */
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == (enum cdb_sc)status);
		t_abort(gv_cur_region, csa);							/* do crit and other cleanup */
		return status;
	}
	blkBase = dirHist.buffaddr;
	assert(GDSV7m == ((blk_hdr_ptr_t)blkBase)->bver);
#	endif
	if (debug_mupip)
		util_out_print("adjusted level !UL directory block @x!@XQ.", TRUE, level, &curr_blk);
	return cdb_sc_normal; /* finished upgrading this block and also any leaf nodes descended from this block */
}
