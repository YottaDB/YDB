/****************************************************************
 *								*
 * Copyright (c) 2021 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

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
#include "gvcst_protos.h"
#include "muextr.h"
#include "memcoherency.h"
#include "sleep_cnt.h"
#include "interlock.h"
#include "hashtab_mname.h"
#include "wcs_flu.h"
#include "jnl.h"

/* Prototypes */
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
#include "db_header_conversion.h"
#include "anticipatory_freeze.h"
#include "gdsfilext.h"
#include "bmm_find_free.h"
#include "mupip_reorg.h"
#include "gvcst_bmp_mark_free.h"
#include "mu_gv_cur_reg_init.h"
#include "db_ipcs_reset.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "wcs_recover.h"
#include "gvt_inline.h"		/* Before gtmio.h, which includes the open->open64 macro on AIX, which we don't want here. */
#include "gtmio.h"
#include "clear_cache_array.h"
#include "bit_clear.h"
#include "bit_set.h"
#include "gds_blk_upgrade.h"
#include "spec_type.h"

#define OLD_MAX_BT_DEPTH	7
#define LEVEL_CNT		0

GBLREF	boolean_t		mu_reorg_in_swap_blk, mu_reorg_process, mu_reorg_upgrd_dwngrd_in_prog, need_kip_incr;
GBLREF	char			*update_array, *update_array_ptr;	/* for the BLK_* macros */
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey, *gv_currkey, *gv_currkey_next_reorg;
GBLREF	gv_namehead		*gv_target, *gv_target_list, *reorg_gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	int4			blks_needed, gv_keysize;
GBLREF	sgmnt_addrs		*cs_addrs, *kip_csa;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	tp_region		*grlist;
GBLREF	uint4			update_array_size;			/* for the BLK_* macros */
GBLREF	uint4			update_trans;
GBLREF	unsigned char		cw_map_depth, rdfail_detail, t_fail_hist[];
GBLREF	unsigned int		t_tries;

static gtm_int8	blk_moved_cnt, killed_gbl_cnt, root_moved_cnt, tot_dt, tot_kill_block_cnt, tot_kill_byte_cnt, tot_levl_cnt,
	tot_splt_cnt;

error_def(ERR_CPBEYALLOC);
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
	boolean_t		dt, got_standalone, mv_blk_err;	/* TODO:deal with got_standalone  */
	block_id		bml_for_relo, curbml, new_blk_num, old_blk_num, blk_temp;
	blk_hdr			blkHdr;
	cache_rec_ptr_t		cr;
	gtm_int8		bmm_bump, extend;
	gvnh_reg_t		*gvnh_reg;
	int			blks_in_way, *blks_to_mv_levl_ptr[OLD_MAX_BT_DEPTH], currKeySize, cycle, i,
				key_cmpc, key_len, lev, level, rec_sz, save_errno;
	int4			blk_size, blks_in_bml, bml_index, bml_status, bmls_to_work,
				index, last_bml, num_blks_mv, status;
	kill_set		kill_set_list;
	mname_entry		gvname;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blkBase, blkBase2, bml_buff, recBase;
	srch_blk_status		*blkhist, hist, *t1;
	srch_hist		*dir_hist_ptr;
	trans_num		ret_tn;
	unix_db_info		*udi;
	unsigned char		*cp, key_buff[MAX_KEY_SZ + 3];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Setup and check that the region is capable of enlarging the master bitmap */
	csa = cs_addrs;
	csd = cs_data;
	blk_size = csd->blk_size;
	util_out_print("!/Region !AD: MUPIP UPGRADE -MASTERMAP started", TRUE, REG_LEN_STR(reg));
	if (reg_cmcheck(reg))
	{
		util_out_print("Region !AD: MUPIP UPGRADE -MASTERMAP cannot run across network", TRUE, REG_LEN_STR(reg));
		return ERR_MUNOACTION;
	}
	if (0 != memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		util_out_print("Region !AD: MUPIP UPGRADE -MASTERMAP can only be run on a V6 database.", TRUE, REG_LEN_STR(reg));
		return ERR_MUNOACTION;
	}
	assert(START_VBN_V6 == csd->start_vbn);
	 /* blks_in_way - Total number of blocks in area being moved (including non-BUSY blocks)
	 * num_blks_mv - number of blocks actually being moved (only BUSY blocks are moved)
	 * bmls_to_work - number of local bitmaps that are in the area being moved
	 */
	/* Calculate the new starting vbn */
	blks_in_way = ROUND_UP2(((START_VBN_CURRENT - csd->start_vbn) / (blk_size / DISK_BLOCK_SIZE)), BLKS_PER_LMAP);
	assert((csd->start_vbn + (blks_in_way * blk_size) / DISK_BLOCK_SIZE) >= START_VBN_CURRENT);
	blocks_needed += blks_in_way;
	blocks_needed += DIVIDE_ROUND_UP(blocks_needed, BLKS_PER_LMAP - 1);				/* local bit map overhead */
	extend = blocks_needed - csd->trans_hist.free_blocks;				/* extend must be signed for comparison */
	if ((0 < extend) && (SS_NORMAL != (status = upgrade_extend(extend, reg))))			/* WARNING assignment */
	{
		assert(SS_NORMAL == status);
		return status;
	}
	util_out_print(" Continuing with master bitmap extension.", TRUE);
	mu_reorg_upgrd_dwngrd_in_prog = TRUE;
	assert(csd->trans_hist.free_blocks >= extend);
	/* Determine what blocks have to be moved from the start of the database */
	memset(blks_to_mv_levl_ptr, 0, OLD_MAX_BT_DEPTH * SIZEOF(*blks_to_mv_levl_ptr));
	bmls_to_work = blks_in_way / BLKS_PER_LMAP;				/* on a small DB this is overkill, but simpler */
	blkhist = &hist;
	mv_blk_err = FALSE;
	for (curbml = index = old_blk_num = 0; old_blk_num < blks_in_way; curbml += BLKS_PER_LMAP)
	{	/* check which blocks need to move */
		bml_buff = t_qread(curbml, (sm_int_ptr_t)&cycle, &cr);
		if (NULL == bml_buff)
		{
			assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
			mu_reorg_upgrd_dwngrd_in_prog = FALSE;
			util_out_print("Region !AD: Read of Bit map @x!@XQ failed.", TRUE, REG_LEN_STR(reg), &curbml);
			util_out_print("Region !AD: not upgraded", TRUE,REG_LEN_STR(reg));
			return ERR_MUNOFINISH;
		}
		blks_in_bml = (blks_in_way > curbml) ? BLKS_PER_LMAP : 2;		/* in last bml only interested in block 1 */
		for (bml_index = 1; bml_index < blks_in_bml; bml_index++)
		{	/* process the local bit map for BUSY blocks */
			old_blk_num = curbml + bml_index;
			GET_BM_STATUS(bml_buff, bml_index, bml_status);
			assert((blks_in_way != curbml) || (BLK_MAPINVALID != bml_status));
			if (BLK_BUSY == bml_status)
			{	/* this block is BUSY so add it to the array of blocks to move */
				blkBase = t_qread(old_blk_num, &blkhist->cycle, &blkhist->cr);
				if (NULL == blkBase)
				{
					assert(blkBase);
					assert(cdb_sc_normal ==  (enum cdb_sc)rdfail_detail);
					mv_blk_err = TRUE;
					util_out_print("Region !AD: Read of block @x!@XQ failed; moving on to next block.", TRUE,
						REG_LEN_STR(reg), &old_blk_num);
					break;
				}
				index++;
				if (1 == old_blk_num)
					continue;	/* Dealing with the root of the directory tree is special, but count it */
				blkHdr = *((blk_hdr_ptr_t)blkBase);
				level = (int)blkHdr.levl;
				if (NULL == blks_to_mv_levl_ptr[level])
				{	/* allocate pointers for an additional level */
					blks_to_mv_levl_ptr[level] = malloc(SIZEOF(int) * 2 * blks_in_way + 1);
					memset(blks_to_mv_levl_ptr[level], 0, SIZEOF(int) * 2 * blks_in_way + 1);
				}
				blks_to_mv_levl_ptr[level][LEVEL_CNT]++;
				blks_to_mv_levl_ptr[level][old_blk_num] = index;
			}
		}
	}	/* end identification of blocks to move */
	/* TODO: normalize crit management; run in 3rd retry? */
	blk_moved_cnt = root_moved_cnt = 0;
	num_blks_mv = index;
	assert(((blks_in_way) + 1) >= old_blk_num);
	assert(DBKEYSIZE(MAX_KEY_SZ) == gv_keysize);			/* no need to invoke GVKEYSIZE_INIT_IF_NEEDED macro */
	assert(NULL == gv_currkey_next_reorg);
	GVKEY_INIT(gv_currkey_next_reorg, gv_keysize);
	reorg_gv_target = targ_alloc(MAX_KEY_SZ, NULL, NULL);		/* because we're using swap funtionality from reorg */
	reorg_gv_target->hist.depth = 0;
	reorg_gv_target->alt_hist->depth = 0;
	last_bml = DIVIDE_ROUND_UP(csd->trans_hist.total_blks, BLKS_PER_LMAP);
	bml_for_relo = bmls_to_work;
	assert(bml_for_relo < last_bml);		/* if they are equal bmm_find_free starts at 0, which would not be good */
	grab_crit(reg, WS_64);
	csa->hold_onto_crit = TRUE;
	for (level = 0, index = 1; num_blks_mv > index; bml_for_relo++, bml_for_relo = blk_temp)
	{	/* try to move all the blocks found busy */
		blk_temp = bmm_find_free(bml_for_relo, csa->bmm, last_bml);
		assert(blk_temp >= bml_for_relo);
		if (bml_for_relo == blk_temp)
		{	/* bml_for_relo should have free blocks */
			curbml = bml_for_relo * BLKS_PER_LMAP;
			bml_buff = t_qread(curbml, (sm_int_ptr_t)&cycle, &cr);
			if (NULL == bml_buff)
			{	/* Failed to read the indicated block */
				assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
				mu_reorg_upgrd_dwngrd_in_prog = FALSE;
				util_out_print("Region !AD: Read of Bit map @x!@XQ failed.", TRUE, REG_LEN_STR(reg), &curbml);
				util_out_print("Region !AD: not upgraded", TRUE,REG_LEN_STR(reg));
				return ERR_MUNOFINISH;
			}
			blks_in_bml = ((curbml + BLKS_PER_LMAP) > csd->trans_hist.total_blks)
				? (csd->trans_hist.total_blks - curbml): BLKS_PER_LMAP;
			for (bml_index = (bml_for_relo == bmls_to_work) ? 2 : 1; bml_index < blks_in_bml; bml_index++)
			{	/* skip target for master map; loop through a local bit map looking for room to relocate blocks */
				GET_BM_STATUS(bml_buff, bml_index, bml_status);
				if (BLK_BUSY == bml_status)
				{
					assert((bml_index != 1) || (curbml != blks_in_way));
					continue;
				}
				for (new_blk_num = curbml + bml_index; level < OLD_MAX_BT_DEPTH; level++)
				{	/* loop through all levels of blocks relocating to a local bit map
					 * in order to assure finding parents, must move deeper levels first
					 */
					if ((NULL == blks_to_mv_levl_ptr[level]) || (0 == blks_to_mv_levl_ptr[level][LEVEL_CNT]))
						continue;		/* no blocks (left) at this level */
					for (old_blk_num = 2; old_blk_num <= (1 + blks_in_way); old_blk_num++)
					{	/* skipping block 1, loop through blocks of a level relocating to a local bit map */
						if (0 == blks_to_mv_levl_ptr[level][old_blk_num])
							continue;	/* not marked to move while processing this level */
						assert(blks_to_mv_levl_ptr[level][LEVEL_CNT]
							&& blks_to_mv_levl_ptr[level][old_blk_num]);
						blks_to_mv_levl_ptr[level][LEVEL_CNT]--;
						kill_set_list.used = 0;
						assert(!update_trans && !need_kip_incr);
						update_trans = UPDTRNS_DB_UPDATED_MASK;
						t_begin_crit(ERR_MUNOUPGRD);
						blkBase = t_qread(old_blk_num, &blkhist->cycle, &blkhist->cr);
						if (NULL == blkBase)
						{
							assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
							t_abort(gv_cur_region, cs_addrs);	/* do crit and other cleanup */
							mv_blk_err = TRUE;
							util_out_print("Region !AD : Read of block @x!@XQ failed;", FALSE,
								REG_LEN_STR(reg), &old_blk_num);
							util_out_print(" going on to next block.", TRUE);
							continue;
						}
						blkHdr = *((blk_hdr_ptr_t)blkBase);
						if (SIZEOF(blk_hdr) == blkHdr.bsiz)
						{	/* data level of a KILLed global - dealt with later in upgrade_dir_tree */
							assert(0 == blkHdr.levl);
							t_abort(gv_cur_region, cs_addrs);	/* do crit and other cleanup */
							continue;
						}
						blkhist->blk_num = old_blk_num;
						blkhist->buffaddr = blkBase2 = blkBase;
						blkhist->level = blkHdr.levl;
						recBase = blkBase + SIZEOF(blk_hdr);
						gvnh_reg = NULL;	/* to silence [-Wuninitialized] warning */
						status = find_dt_entry_for_blk(blkhist, blkBase2, recBase, &gvname, gvnh_reg);
						if (cdb_sc_normal != status)
						{	/* skip this block */
							if ((cdb_sc_starrecord != status) || (1 != blkHdr.levl))
							{	/* not a killed global artifact */
								mv_blk_err = TRUE;
								util_out_print("Region !AD: directory lookup failed, preventing ",
									FALSE, REG_LEN_STR(reg));
								util_out_print(" move of @x!@XQ; going on to next block.", TRUE,
									&old_blk_num);
							}
							t_abort(gv_cur_region, cs_addrs);	/* do crit and other cleanup */
							continue;
						}
						dt = FALSE;
						for (lev = gv_target->hist.depth - 1; 0 <= lev ; lev--)
						{	/* check if the block is in the directory tree */
							if (old_blk_num == gv_target->hist.h[lev].blk_num)
							{	/* it is */
								assert(blkHdr.levl == lev);
								dt = TRUE;
								break;
							}
						}
						if (!dt)
						{	/* get the target global tree */
							recBase = gv_target->hist.h[0].buffaddr
								+ gv_target->hist.h[0].curr_rec.offset;
							memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
							status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff,
								blkHdr.levl, &(gv_target->hist.h[0]), recBase);
							if ((cdb_sc_normal != status))
							{	/* failed to parse directory tree record for desired key */
								assert(cdb_sc_normal == status);
								t_abort(gv_cur_region, cs_addrs); /* do crit and other cleanup */
								mv_blk_err = TRUE;
								util_out_print("Region !AD: parse of directory for key !AD failed ",
									FALSE, REG_LEN_STR(reg), gvname.var_name.len,
									gvname.var_name.addr);
								util_out_print("preventing move of 0x!@XQ; going on to next block.",
									TRUE, &old_blk_num);
								continue;
							}
							/*verify expanded key matches the name */
							assert(0 == memcmp(gvname.var_name.addr, gv_currkey->base,
								gvname.var_name.len));
							READ_BLK_ID(BLKID_32, &(gv_target->root),
								recBase + SIZEOF(rec_hdr) + key_len);
							gv_target->clue.end = 0;
							if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
							{	/* failed to get global tree history for key; WARNING: assgn above*/
								assert(cdb_sc_normal == status);
								PRO_ONLY(UNUSED(status));
								t_abort(gv_cur_region, cs_addrs); /* do crit and other cleanup */
								mv_blk_err = TRUE;
								util_out_print("Region !AD: search for for key !AD failed, so ",
									FALSE, REG_LEN_STR(reg), gvname.var_name.len,
									gvname.var_name.addr);
								util_out_print("can't move block 0x!@XQ; going on to next block.",
									TRUE, &old_blk_num);
								continue;
							}
						}
						/* Step 3: Move the block to its new location */
						reorg_gv_target->hist.depth = 0;
						reorg_gv_target->alt_hist->depth = 0;
						if (dt || (blkHdr.levl != gv_target->hist.depth))
						{	/* The block being moved is not a root block */
							assert(old_blk_num == gv_target->hist.h[blkHdr.levl].blk_num);
							/* mu_swap_blk increments the destination block hint to find a block to swap
							 * into; that logic was designed for a regular reorg operation and is not
							 * relevant to this case, so decrement the selected location into a hint
							 * landing us on the desired block
							 */
							new_blk_num--;
							mu_reorg_in_swap_blk = TRUE;
							status = mu_swap_blk(blkHdr.levl, &new_blk_num, &kill_set_list, NULL,
								old_blk_num);
							mu_reorg_in_swap_blk = FALSE;
							if (cdb_sc_normal != status)
							{
								assert(cdb_sc_normal == status);
								t_abort(gv_cur_region, cs_addrs); /* do crit and other cleanup */
								mv_blk_err = TRUE;
								util_out_print("Region !AD: failed swap of block 0x!@XQ to 0x!@XQ;",
									FALSE, REG_LEN_STR(reg), &old_blk_num, new_blk_num);
								util_out_print(" going on to next block.", TRUE);
								continue;
							}
							assert(blks_in_way + 1 != new_blk_num);
							assert(new_blk_num == cw_set[0].blk);
							assert(WAS_FREE(cw_set[0].blk_prior_state));
							assert(!WAS_FREE(cw_set[1].blk_prior_state));
							assert(ROUND_DOWN2(new_blk_num, BLKS_PER_LMAP) == cw_set[2].blk);
							gv_target->hist.h[0] = reorg_gv_target->hist.h[level];
							gv_target->hist.h[0].cse = & cw_set[0];
							ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED);
							if (0 == ret_tn)
							{
								assert(ret_tn);
								t_abort(gv_cur_region, cs_addrs); /* do crit and other cleanup */
								mv_blk_err = TRUE;
								util_out_print("Region !AD: failed commit of block 0x!@XQ; ",
									FALSE, REG_LEN_STR(reg), &old_blk_num);
								util_out_print("swap to 0x@XQ going on to next block.", TRUE,
									&new_blk_num);
								continue;
							}
							assert((1 == kill_set_list.used) && !kill_set_list.blk[0].level
							&& !kill_set_list.blk[0].flag
							&& (old_blk_num == kill_set_list.blk[0].block));
							GVCST_BMP_MARK_FREE(&kill_set_list, ret_tn, inctn_opcode,
								inctn_bmp_mark_free_mu_reorg, inctn_opcode, csa);
							kill_set_list.used = 0;
							if (0 == ret_tn)
							{
								assert(ret_tn);
								t_abort(gv_cur_region, cs_addrs); /* do crit and other cleanup */
								mv_blk_err = TRUE;
								util_out_print("Region !AD: failed to free block 0x!@XQ; ", FALSE,
									REG_LEN_STR(reg), &old_blk_num);
								util_out_print("going on to next block; this only matters ", FALSE);
								util_out_print("if another type of problem stops region conversion",
									TRUE);
							}
							DBGUPGRADE(util_out_print("moved level !UL block 0x!@XQ to 0x!@XQ", TRUE,
								blkHdr.levl, &old_blk_num, &new_blk_num));
								blk_moved_cnt++;
						} else	/* The block being moved is a root block */
						{
							move_root_block(--new_blk_num, old_blk_num, gvnh_reg, &kill_set_list);
							DBGUPGRADE(util_out_print("moved level !UL root block 0x!@XQ to 0x!@XQ",
								TRUE, blkHdr.levl, &old_blk_num, &new_blk_num));
							new_blk_num++;
							root_moved_cnt++;
						}
						INCR_BLK_NUM(new_blk_num);
						t_abort(gv_cur_region, cs_addrs);	/* do crit and other cleanup */
					}	/* loop through blocks of a level relocating to a local bit map */
				}	/* loop through all levels of blocks relocating to a local bit map */
				/* no before_image of formerly used blocks as this boundry requires journal restart */
				if (num_blks_mv <= ++index)
					break;						/* all blocks relocated except DIR_ROOT */
			}	/* loop through a local bit map looking for room to relocate blocks */
			break;
		} else if (blk_temp < bml_for_relo)
		{	/* This should never loop around but could try another extension */
			assert(blk_temp >= bml_for_relo);
			mu_reorg_upgrd_dwngrd_in_prog = FALSE;
			util_out_print("Region !AD: ran out of free blocks after identifying !UL.", TRUE,
				REG_LEN_STR(reg),index - 1);
			util_out_print("Region !AD: not upgraded", TRUE,REG_LEN_STR(reg));
			return ERR_MUNOFINISH;
		}
	}	/* all but the DIR_ROOT moved */
	for (level = 0; level < OLD_MAX_BT_DEPTH; level++)
	{
		if (NULL != blks_to_mv_levl_ptr[level])
			free(blks_to_mv_levl_ptr[level]);
	}
	/* Check if there were any errors while moving the blocks */
	if (TRUE == mv_blk_err)
	{	/* one or more errors while moving blocks, so don't process DT or move block 1 (DIR_ROOT) to new location
		 * just print an error and move on to the next region; TODO: test to verify
		 */
		t_abort(gv_cur_region, cs_addrs);					  /* do crit and other cleanup */
		mu_reorg_upgrd_dwngrd_in_prog = FALSE;
		util_out_print("Region !AD: Not all blocks were moved successfully; ", FALSE, REG_LEN_STR(reg));
		util_out_print("cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	csd->blks_to_upgrd = csd->trans_hist.total_blks;
	gv_target_list = NULL;
	memset(&hist, 0, SIZEOF(hist));								/* null-initialize history */
	old_blk_num = blkhist->blk_num = DIR_ROOT;
	new_blk_num = blks_in_way + 1;	/* step past the local bit map */
	gv_target = targ_alloc(csa->hdr->max_key_size, &gvname, reg);
	gv_target->root = DIR_ROOT;
	gv_target->clue.end = 0;
	blkBase = t_qread(old_blk_num, &blkhist->cycle, &blkhist->cr);
	if (NULL == blkBase)
	{
		assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_reorg_upgrd_dwngrd_in_prog = FALSE;
		util_out_print("Region !AD: read of block @x!@XQ failed. ", FALSE, REG_LEN_STR(reg), &old_blk_num);
		util_out_print(" Cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	blkHdr = *((blk_hdr_ptr_t)blkBase);
	gv_target->hist.depth = blkHdr.levl;
	gv_target->hist.h[blkHdr.levl].buffaddr = blkBase;
	gv_target->hist.h[blkHdr.levl].blk_num = gv_target->root = DIR_ROOT;
	killed_gbl_cnt = 0;
	if (cdb_sc_normal != (status = ditch_dead_globals(DIR_ROOT)))			/* WARNING assignment */
	{
		assert(cdb_sc_normal == status);
		PRO_ONLY(UNUSED(status));
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_reorg_upgrd_dwngrd_in_prog = FALSE;
		util_out_print("Region !AD: directory processing KILL'd globals failed; ", FALSE, REG_LEN_STR(reg));
		util_out_print("cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
	DBGUPGRADE(util_out_print("moving level: !UL root block 0x!@XQ to 0x!@XQ", TRUE, blkHdr.levl, &old_blk_num, &new_blk_num));
	memcpy(gv_currkey->base, gvname.var_name.addr, gvname.var_name.len);
	gv_currkey->end = gvname.var_name.len;
	gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
	gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
	gv_altkey = gv_currkey;
	assert(!update_trans && !need_kip_incr);
	update_trans = UPDTRNS_DB_UPDATED_MASK;
	t_begin_crit(ERR_MUNOUPGRD);
	if (SS_NORMAL != move_root_block(--new_blk_num, old_blk_num, gvnh_reg, &kill_set_list))
	{
		assert(SS_NORMAL == status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_reorg_upgrd_dwngrd_in_prog = FALSE;
		util_out_print("Region !AD: move of directory tree root failed; ", FALSE, REG_LEN_STR(reg));
		util_out_print("cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	root_moved_cnt++;
	reorg_gv_target->gvname.var_name = gv_target->gvname.var_name;			/* needed by SAVE_ROOTSRCH_ENTRY_STATE */
	t_abort(gv_cur_region, cs_addrs); 							/* do crit and other cleanup */
	assert(DIR_ROOT == gv_target->hist.h[blkHdr.levl].blk_num);
	gv_target->hist.h[blkHdr.levl].blk_num += blks_in_way;
	/* Update the header to reflect the new svbn */
	wcs_flu(WCSFLU_NONE);									/* push it all out */
	csd->fully_upgraded = FALSE;
	csd->desired_db_format = GDSV6p;				/* adjust version temporarily to accomodate upgrade of DT */
	csd->offset = blks_in_way;				/* offset serves to adjust index pointers in pre-upgrade blks */
	csd->trans_hist.total_blks -= blks_in_way;
	csd->trans_hist.free_blocks -= (blks_in_way - bmls_to_work);
	csd->blks_to_upgrd -= (blks_in_way + csd->trans_hist.free_blocks - 1);
	csd->start_vbn += (blks_in_way * blk_size) / DISK_BLOCK_SIZE;		/* upgraded file has irregular start_vbn */
	csd->free_space = (blks_in_way * blk_size) - SIZEOF_FILE_HDR_DFLT;
	wcs_flu(WCSFLU_IN_COMMIT | WCSFLU_FLUSH_HDR | WCSFLU_CLEAN_DBSYNC);
	/* abusing following function to clear the entire array; last argument is not currently used by the function */
	clear_cache_array(csa, csd, gv_cur_region, (block_id)0, csd->trans_hist.total_blks - blks_in_way);
	SYNC_ROOT_CYCLES(csa);
	wcs_recover(reg);
	inctn_opcode = inctn_invalid_op;
	assert((0 == cs_data->kill_in_prog) && (NULL == kip_csa));
	SYNC_ROOT_CYCLES(csa);
	CHECK_AND_RESET_UPDATE_ARRAY;						/* reset update_array_ptr to update_array */
	util_out_print("Region !AD : Master map required 0x!@XQ blocks succeeded.", FALSE, REG_LEN_STR(reg), &blks_in_way);
	util_out_print(" size is now at 0x!@XQ blocks.", TRUE, &cs_data->trans_hist.total_blks);
	if (cdb_sc_normal != (status = clean_master_map())) /* TODO: sb unnecessary find way to ditch *//* WARNING assignment */
	{
		assert(cdb_sc_normal == status);
		PRO_ONLY(UNUSED(status));
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_reorg_upgrd_dwngrd_in_prog = FALSE;
		util_out_print("Region !AD: mastermap adjustment failed; ", FALSE, REG_LEN_STR(reg));
		util_out_print("cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	assert(FALSE == csd->fully_upgraded);
	tot_dt = tot_kill_block_cnt = tot_kill_byte_cnt = tot_levl_cnt = tot_splt_cnt = 0;
	/* in V7.1 the following does both the offest adjustment and the pointer enlargement */
	if (cdb_sc_normal != (status = upgrade_dir_tree(DIR_ROOT, blks_in_way, csd->blk_size, reg)))	/* WARNING assignment */
	{
		assert(cdb_sc_normal == status);
		PRO_ONLY(UNUSED(status));
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_reorg_upgrd_dwngrd_in_prog = FALSE;
		util_out_print("Region !AD: directory tree upgrade failed; ", FALSE, REG_LEN_STR(reg));
		util_out_print("cannot complete mastermap enlargement.", TRUE);
		return ERR_MUNOFINISH;
	}
	INCR_BLKS_TO_UPGRD(csa, csd, tot_splt_cnt + tot_levl_cnt);
	bmm_bump = (DIVIDE_ROUND_UP(MASTER_MAP_SIZE_DFLT - csd->master_map_len, DISK_BLOCK_SIZE)) * DISK_BLOCK_SIZE;
	assert(MASTER_MAP_SIZE_DFLT == bmm_bump + csd->master_map_len);	/* the DIVIDE_ROUND_UP should not have been necessary */
	blkBase = malloc(bmm_bump);
	memset(blkBase, BMP_EIGHT_BLKS_FREE, bmm_bump); 					/* initialize additional bmm */
	udi = FILE_INFO(reg);							/* write added master map initiatlization */
	DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, SIZEOF_FILE_HDR(csd), blkBase, bmm_bump, status); /* macro: file_hdr + old bmm */
	if (SS_NORMAL != status)
	{
		save_errno = errno;
		assert(SS_NORMAL == status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		mu_reorg_upgrd_dwngrd_in_prog = FALSE;
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(13) ERR_DBFILERR, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("Error initializing additional master bit map space"),
			ERR_TEXT, 2, LEN_AND_STR(STRERROR(save_errno)));
		return ERR_MUNOFINISH;
	}
	free(blkBase);
	csd->master_map_len = MASTER_MAP_SIZE_DFLT;
	csd->desired_db_format = GDSV7m;		/* adjust version to match that of any future master map expansion */
	csd->desired_db_format_tn = csd->reorg_db_fmt_start_tn = csd->trans_hist.curr_tn - 1;
	csd->minor_dbver = GDSMV70001;
	csd->tn_upgrd_blks_0 = csd->reorg_upgrd_dwngrd_restart_block = csd->blks_to_upgrd_subzero_error =  0;
	csd->db_got_to_V7_once = FALSE;
	db_header_dwnconv(csd);					/* revert the file header to V6 format so we can save it */
	db_header_upconv(csd);								/* finish transition to new header */
	wcs_flu(WCSFLU_IN_COMMIT | WCSFLU_FLUSH_HDR | WCSFLU_CLEAN_DBSYNC);
	/* Finished region.  Cleanup for move onto next region. */
	csa->hold_onto_crit = FALSE;
	rel_crit(reg);
	mu_reorg_upgrd_dwngrd_in_prog = FALSE;
	util_out_print("Region !AD : MUPIP UPGRADE -MASTERMAP completed.", TRUE, REG_LEN_STR(reg));
	util_out_print("Relocated 0x!@XQ root blocks and 0x!@XQ other blocks", TRUE, &root_moved_cnt, &blk_moved_cnt);
	util_out_print("Freed 0x!@XQ blocks and 0x!@XQ directory tree bytes from 0x!@XQ KILL'd globals", TRUE, &tot_kill_block_cnt,
		&tot_kill_byte_cnt, &killed_gbl_cnt);
	util_out_print("Upgraded 0x!@XQ directory tree blocks while splitting 0x!@XQ blocks, adding 0x!@XQ directory tree levels",
		TRUE, &tot_dt, &tot_splt_cnt, &tot_levl_cnt);
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

	util_out_print("Region !AD: Not enough free blocks to extend the master map & provide additional index blocks.",
		FALSE, REG_LEN_STR(reg));
	if (0 == cs_data->extension_size)
	{
		assert(cs_data->extension_size);
		util_out_print(" Extension size not set in database header.", TRUE);
		util_out_print("Region !AD: Perform a MUPIP EXTEND on this region,", FALSE, REG_LEN_STR(reg));
		util_out_print(" otherwise free at least 0x!@XQ blocks to continue.", TRUE, &extension);
		return ERR_MUNOFINISH;
	}
	util_out_print(" Attempting a file extension on the database.", TRUE);
	status = GDSFILEXT(extension, cs_data->trans_hist.total_blks, TRANS_IN_PROG_FALSE);
	if (SS_NORMAL != status)
	{
		assert(SS_NORMAL == status);
		util_out_print("Region !AD: File extension of 0x!@XQ blocks failed.", TRUE, REG_LEN_STR(reg), &extension);
		util_out_print("Region !AD: not upgraded", TRUE, REG_LEN_STR(reg));
		return ERR_MUNOFINISH;
	}
	util_out_print("Region !AD : File extension of 0x!@XQ blocks succeeded.", FALSE, REG_LEN_STR(reg), &extension);
	util_out_print(" size temporarily at 0x!@XQ blocks.", TRUE, &cs_data->trans_hist.total_blks);
	return SS_NORMAL;
}

/******************************************************************************************
 * This helper function, given a block, and record attempts to create a name and history
 * structure by fiinding a name and searching for it; we need this because, unlike normal
 * database operations we are start with the block rather than the name; the code is similar
 * in approach to that in DSE when the operator initiates an operation identify by a block
 *
 * Input Parameters:
 *	blkBase2 points to a block holding a record (typically the first in the block)
 *	recBase points to record in blkBase2 the caller wishes to follow
 * Output Parameters:
 *	gvname points to a structure for storing the name selected for which to search
 *	blkhist points to a structure containing the history from the search
 *	(enum cdb_sc) returns a code containing cdb_sc_normal or a "retry" code
 ******************************************************************************************/
enum cdb_sc find_dt_entry_for_blk(srch_blk_status *blkhist, sm_uc_ptr_t blkBase2, sm_uc_ptr_t recBase, mname_entry *gvname,
				  gvnh_reg_t *gvnh_reg)
{	/* given a block, find a useful key */
	block_id		blk_temp;
	boolean_t		long_blk_id;
	int			currKeySize, i, key_cmpc, key_len, rec_sz;
	int4			status;
	unsigned char		*cp, key_buff[MAX_KEY_SZ + 1];

	long_blk_id = IS_64_BLK_ID(blkBase2);
	do
	{
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, blkhist->level, blkhist, recBase);
		if (cdb_sc_starrecord ==  status)
		{	/* 1st record in block is a star key - drop a level & try again */
			assert(blkhist->level);
			READ_BLK_ID(long_blk_id, &blk_temp, SIZEOF(rec_hdr) + recBase + key_len);
			blkBase2 = t_qread(blk_temp, &blkhist->cycle, &blkhist->cr);
			long_blk_id = IS_64_BLK_ID(blkBase2);
			if (NULL == blkBase2)
			{	/* Failed to read the indicated block */
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				break;
			}
			if (SIZEOF(blk_hdr) != ((blk_hdr_ptr_t)blkBase2)->bsiz)
			{	/* block is not empty */
				currKeySize = get_key_len(blkBase2, blkBase2 + SIZEOF(blk_hdr) + SIZEOF(rec_hdr));
				memcpy(key_buff, blkBase2 + SIZEOF(blk_hdr) + SIZEOF(rec_hdr), currKeySize);
				status = cdb_sc_normal;
			} else
			{ 	/* killed global tree */
				assert(0 == ((blk_hdr_ptr_t)blkBase2)->levl);
				break;	/* move on */
			}
		} else if (cdb_sc_normal != status)
		{	/* failed to parse record */
			assert(cdb_sc_normal == status);
			break;
		} else
		{	/* block has a potentially useful key in the first record */
			currKeySize = key_len;
			assert(MAX_KEY_SZ >= currKeySize);
			assert(!memcmp(key_buff, blkBase2 + SIZEOF(blk_hdr) + SIZEOF(rec_hdr), currKeySize));
		}
		assert((0 != blkhist->level) || (bstar_rec_size(long_blk_id) != ((blk_hdr_ptr_t)blkBase2)->bsiz));
		/* parse for valid unsubscripted name; skip 1st character - can be % */
		for (cp = key_buff + 1, i = currKeySize - 1; (0 < i) && VALKEY(*cp); cp++, i--)
			;
		if ((KEY_DELIMITER == *cp++) && (KEY_DELIMITER == *cp--))
		{	/* usually KEY_DELIMITER */
			gvname->var_name.len = cp - key_buff;
			assert(gvname->var_name.len + 2 == currKeySize);
			break;
		}	/* but if weird character from block split, drop another level */
		READ_BLK_ID(long_blk_id, &blk_temp, (sm_uc_ptr_t)(blkBase2 + SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) + key_len);
		blkBase2 = t_qread(blk_temp, &blkhist->cycle, &blkhist->cr);
		if (NULL == blkBase2)
		{
			assert(cdb_sc_normal == (enum cdb_sc)rdfail_detail);
			status = rdfail_detail;
			break;
		}
		long_blk_id = IS_64_BLK_ID(blkBase2);
		blkhist->level = ((blk_hdr_ptr_t)blkBase2)->levl;
		blkhist->buffaddr = blkBase2;
		recBase = blkBase2 + SIZEOF(blk_hdr);
	} while (TRUE);
	if (cdb_sc_normal == status)
	{
		gvname->var_name.len = cp - key_buff;
		gvname->var_name.addr = (char *)key_buff;
		memcpy(gv_currkey->base, key_buff, currKeySize + 1);				/* the +1 gets a key_delimiter */
		gv_currkey->end = currKeySize - 1;
		COMPUTE_HASH_MNAME(gvname);
		GV_BIND_NAME_ONLY(gd_header, gvname, gvnh_reg);
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
		gv_altkey = gv_currkey;
		gv_target->clue.end = 0;
		gv_target->root = DIR_ROOT;
		for (i = 0; i < MAX_BT_DEPTH; i++)
			gv_target->hist.h[i].level = i;
		status = gvcst_search(gv_currkey, NULL);
		assert(cdb_sc_normal == status);
	}
	return status;
}

/******************************************************************************************
 * This interlude function mediates invocation of mu_swap_root for global trees and the
 * directory tree root; mu_swap_root relies on the retry mechanism, and is otherwise opaque
 * with respect to errors, calls to it currently aways return SS_NORMAL
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
	block_id		did_block, bml_blk, total_blks;
	cache_rec_ptr_t		dummy_cr;
	glist			root_tag;
	int			root_swap_stat;
	int4			dummy_int, status;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		bp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	csd = cs_data;
	root_tag.reg = gv_cur_region;
	root_tag.gvt = gv_target;
	root_tag.gvnh_reg = gvnh_reg;
	update_trans = 0;
	mu_reorg_process = TRUE;
	assert(mu_reorg_upgrd_dwngrd_in_prog);
	status = SS_NORMAL;
	if (DIR_ROOT != old_blk_num)
	{
		mu_swap_root(&root_tag, &root_swap_stat, new_blk_num++);     /* TODO: can we do better with error handling here? */
		bm_setmap(ROUND_DOWN(new_blk_num, BLKS_PER_LMAP), new_blk_num, TRUE);	/* desperate hack to mark root busy */
	} else
	{
		assert(0 == csd->kill_in_prog);
		did_block = mu_swap_root_blk(&root_tag, &(gv_target->hist), gv_target->alt_hist, kill_set_list,
					     csa->ti->curr_tn, new_blk_num++);
		if (did_block != new_blk_num)
		{
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
	mu_reorg_process = FALSE;
	/* the following is a hack to deal with bitmap maintenance which should be done a better way */
	bm_setmap(ROUND_DOWN(new_blk_num, BLKS_PER_LMAP), new_blk_num, TRUE);
#ifdef notnow
	bml_blk = new_blk_num / BLKS_PER_LMAP * BLKS_PER_LMAP;
	if (dba_mm == csd->acc_meth)
		bp = MM_BASE_ADDR(csa) + (off_t)bml_blk * csd->blk_size;
	else
	{
		assert(dba_bg == csd->acc_meth);
		if (!(bp = t_qread(bml_blk, &dummy_int, &dummy_cr)))
		{
			status = (enum cdb_sc)rdfail_detail;
			assert(cdb_sc_normal == (enum cdb_sc)status);
			return status;
		}
	}
	if (NO_FREE_SPACE == bml_find_free(0, bp + SIZEOF(blk_hdr), (((csa->ti->total_blks / BLKS_PER_LMAP)
			* BLKS_PER_LMAP) == bml_blk) ? (csa->ti->total_blks - bml_blk) : BLKS_PER_LMAP))
		bit_clear(bml_blk / BLKS_PER_LMAP, csa->bmm);
	else
		bit_set(bml_blk / BLKS_PER_LMAP, csa->bmm);
	if (bml_blk > csa->nl->highest_lbm_blk_changed)
		csa->nl->highest_lbm_blk_changed = bml_blk;
#endif
	return status;
}

/******************************************************************************************
 * This helper function, recreates the master map required due to prior sloppiness with
 * bit map maintenance TODO -fix the maintenance so this is not required
 *
 * Input Parameters:
 *	None
 * Output Parameters:
 *	(enum cdb_sc) cdb_sc_normal (or hopefully not) a retry code
 ******************************************************************************************/
enum cdb_sc clean_master_map(void)
{
	block_id		blk_index, bml_index, total_blks;
	int4			blks_in_bitmap, status;
	sgmnt_addrs		*csa;
	srch_blk_status		bml_hist;

	csa = cs_addrs;
	total_blks = csa->ti->total_blks;
	for (blk_index = 0, bml_index = 0;  blk_index < total_blks; blk_index += BLKS_PER_LMAP, bml_index += BLKS_PER_LMAP)
	{
		/* (total_blks - blk_index) is used to determine the number of blks in the last lmap of the DB
		 * so the value should never be larger then BLKS_PER_LMAP and thus fit in a int4
		 */
		assert((blk_index + BLKS_PER_LMAP <= total_blks) || (BLKS_PER_LMAP >= (total_blks - blk_index)));
		blks_in_bitmap = (blk_index + BLKS_PER_LMAP <= total_blks) ? BLKS_PER_LMAP : (int4)(total_blks - blk_index);
		assert(1 < blks_in_bitmap);	/* the last valid block in the database should never be a bitmap block */
		if (NULL == (bml_hist.buffaddr = t_qread(bml_index, (sm_int_ptr_t)&bml_hist.cycle, &bml_hist.cr)))
		{	/* Failed to read the indicated block */
			status = (enum cdb_sc)rdfail_detail;
			assert(cdb_sc_normal == (enum cdb_sc)status);
			return status;
		}
		if (NO_FREE_SPACE != bml_find_free(0, bml_hist.buffaddr + SIZEOF(blk_hdr), blks_in_bitmap))
			bit_set(blk_index / BLKS_PER_LMAP, csa->bmm);
		else
			bit_clear(blk_index / BLKS_PER_LMAP, csa->bmm);
		if (blk_index > csa->nl->highest_lbm_blk_changed)
			csa->nl->highest_lbm_blk_changed = blk_index;
	}
	return cdb_sc_normal;
}

/******************************************************************************************
 * This identifies "KILL'd" globals, marks the root block and the empty data block free and
 * points the directory entry to the start_vbn by giving it the value of the offset
 * upgrade_dir_tree then recognizes these entries as disposable and tosses their records
 *
 * Input Parameters:
 *	curr_blk: block_id on which the (recursive) processing works
 * Output Parameters
 *	(enum) cdb_sc indicating cdb_sc_normal if all goes well, which we count on
 ******************************************************************************************/
enum cdb_sc ditch_dead_globals(block_id curr_blk)
{
	blk_hdr_ptr_t		rootBase;
	blk_segment		*bs1, *bs_ptr;
	block_id		blk_pter, offset;
	boolean_t		long_blk_id;
	int			blk_seg_cnt, blk_size, blk_sz, key_cmpc, key_len, rec_sz;
	int4			status;
	sm_uc_ptr_t		blkBase, blkEnd, recBase;
	srch_blk_status		dirHist, leafHist, rootHist;
	unsigned char		key_buff[MAX_KEY_SZ + 1];

	dirHist.blk_num = curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == (enum cdb_sc)status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		return status;								/* Failed to read the indicated block */
	}
	blk_size = cs_data->blk_size;
	offset = cs_data->offset;
	blkBase = dirHist.buffaddr;
	blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
	blkEnd = blkBase + blk_sz;
	dirHist.level = ((blk_hdr_ptr_t)blkBase)->levl;
	long_blk_id = FALSE;
	assert(GDSV6 == ((blk_hdr_ptr_t)blkBase)->bver);
	/* First get a the key from the first record. This will be used to get a search history for the block */
	for (recBase = blkBase + SIZEOF(blk_hdr); recBase < blkEnd; recBase += rec_sz)
	{	/* iterate through block counting records */
		if (recBase == blkBase)
			break;
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, dirHist.level, &dirHist, recBase);
		if (cdb_sc_starrecord == status)
		{
			assert((((rec_hdr_ptr_t)recBase)->rsiz + recBase == blkEnd) && (0 != dirHist.level));
			gv_target->gvname.var_name = reorg_gv_target->gvname.var_name;
			memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
			key_len = 0;
		} else if (cdb_sc_normal != status)
		{	/* failed to parse record */
			assert(cdb_sc_normal == status);
			return status;
		}
		READ_BLK_ID(long_blk_id, &blk_pter, SIZEOF(rec_hdr) + recBase + key_len);
		if (0 != dirHist.level)
		{	/* this isn't a level 0 block, so traverse down another level */
			if (cdb_sc_normal != (status = ditch_dead_globals(blk_pter)))
			{	/* recursion failed */
				assert(cdb_sc_normal == status);
				t_abort(gv_cur_region, cs_addrs);			/* do crit and other cleanup */
				return status;
			}
		} else
		{	/* Check the global tree to see if this record is for a killed global */
			rootHist.blk_num = blk_pter;
			if (NULL == (rootHist.buffaddr = t_qread(rootHist.blk_num, (sm_int_ptr_t)&rootHist.cycle, &rootHist.cr)))
			{	/* Failed to read the indicated block */
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				return status;
			}
			rootBase = (blk_hdr_ptr_t)(rootHist.buffaddr);
			if ((1 != rootBase->levl) || ((SIZEOF(blk_hdr) + bstar_rec_size(BLKID_32)) != rootBase->bsiz))
			{	/* root node not level 1 or isn't just a star record so not a killed global tree */
				continue;
			}
			READ_BLK_ID(BLKID_32, &(leafHist.blk_num), rootHist.buffaddr + SIZEOF(blk_hdr) + SIZEOF(rec_hdr));
			if (NULL == (leafHist.buffaddr = t_qread(leafHist.blk_num, (sm_int_ptr_t)&leafHist.cycle, &leafHist.cr)))
			{	/* Failed to read the indicated block */
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				return status;
			}
			if (SIZEOF(blk_hdr) != ((blk_hdr_ptr_t)leafHist.buffaddr)->bsiz)
			{	/* This leaf node is not empty so this cannot be a killed global tree */
				continue;
			}
			/* global killed- free its two blocks & whack its pointer */
			inctn_opcode = inctn_bmp_mark_free_mu_reorg;
			bm_setmap(ROUND_DOWN2(leafHist.blk_num, BLKS_PER_LMAP), leafHist.blk_num, FALSE);
			bm_setmap(ROUND_DOWN2(rootHist.blk_num, BLKS_PER_LMAP), rootHist.blk_num, FALSE);
			CHECK_AND_RESET_UPDATE_ARRAY;					/* reset update_array_ptr to update_array */
			BLK_INIT(bs_ptr, bs1);
			//BLK_ADDR(blkBase, blk_sz, unsigned char);
			BLK_SEG(bs_ptr, blkBase + SIZEOF(blk_hdr), blk_sz - SIZEOF(blk_hdr));
			/* pointer given offset so becomes zero with the adjustment*/
			MEMCP(recBase, &offset, SIZEOF(rec_hdr) + key_len, SIZEOF(offset), blk_sz);
			if (!BLK_FINI(bs_ptr, bs1))
			{	/* failed to finalize the update */
				status = cdb_sc_mkblk;
				assert(cdb_sc_normal == status);
				t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
				return status;
			}
			t_write(&dirHist, (unsigned char *)bs1, 0, 0, dirHist.level, TRUE, TRUE, GDS_WRITE_KILLTN);
			inctn_opcode = inctn_mu_reorg;
			if ((trans_num)0 == t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED)) /* WARNING: assign */
			{	/* failed to commit update */
				status = t_fail_hist[t_tries - 1];
				assert(cdb_sc_normal == status);
				return status;
			}
			killed_gbl_cnt++;
		}
	}
	return cdb_sc_normal;
}

/******************************************************************************************
 * This recursively traverses the directory tree and upgrades the pointers, including those
 * in level 0 blocks which hold data except in the directory tree; this approach is key to
 * addressing the issue of recognizing those level 0 blocks that hold pointers so dsk_read
 * does not have to expend resources doing so; it also deals with KILL'd global vestigages
 * by eliminating their entries in the directory tree while this code has sole control of
 * the database file; as mentioned earlier, it upgrages all the pointers from 32-bit to 64-
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
 * 	(enum_cdb_sc) returns cdb_sc_normal which the code expects or a retry code
 ******************************************************************************************/
enum cdb_sc upgrade_dir_tree(block_id curr_blk, block_id offset, int4 blk_size, gd_region *reg)
{
	blk_hdr_ptr_t		rootBase;
	blk_segment		*bs1, *bs_ptr;
	block_id		blk_pter;
	boolean_t		long_blk_id;
	enum db_ver		blk_ver;
	gvnh_reg_t		*gvnh_reg;
	int			blk_seg_cnt, level, split_blks_added, split_levels_added, key_cmpc, key_len, new_blk_sz, num_recs,
				rec_sz, space_need, v7_rec_sz;
	int4			*desperation, status;
	mname_entry		gvname;
	sm_uc_ptr_t		blkBase, blkEnd, recBase, v7bp, v7recBase;
	srch_blk_status		dirHist, leafHist, rootHist;
	trans_num		ret_tn;
	unsigned char		key_buff[MAX_KEY_SZ + 1];

	dirHist.blk_num = curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{	/* Failed to read the indicated block */
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == (enum cdb_sc)status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		return status;
	}
	blkBase = dirHist.buffaddr;
	new_blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
	blkEnd = blkBase + new_blk_sz;
	dirHist.level = level = ((blk_hdr_ptr_t)blkBase)->levl;
	long_blk_id = IS_64_BLK_ID(blkBase);
	blk_ver = ((blk_hdr_ptr_t)blkBase)->bver;
	recBase = blkBase + SIZEOF(blk_hdr);
	/* the directory tree needs its index pointers upgraded to V7 format */
	/* First get a the key from the first record. This will be used to get a search history for the block */
	/* Second check how much space is need to upgrade the block and whether the block will need to be split */
	if (0 == level)
	{	/* data (level 0) blocks require some attention */
		if (GDSV7m == blk_ver)
			return cdb_sc_normal;				/* no need to reprocess an already upgraded level 0 block */
		if (GDSV6 == blk_ver)
		{	/* dsk_read doesn't adjust data blocks - do it here; like blk_ptr_adjust, but necessarily less efficient */
			assert(IS_64_BLK_ID(blkBase) == FALSE);
			DBG_VERIFY_ACCESS(blkEnd - 1);
			for (;recBase < blkEnd; recBase += ((rec_hdr_ptr_t)recBase)->rsiz)
			{	/* iterate through block updating pointers with the offset */
				assert(blkEnd > (recBase + SIZEOF(rec_hdr)));
				status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, dirHist.level, &dirHist, recBase);
				if (cdb_sc_normal != status)
				{	/* no star records in data blocks */
					assert((cdb_sc_normal == status) || (cdb_sc_starrecord == status));
					return status;
				}
				/* Read in the current block pointer & apply the offset preserving the collation block */
				GET_BLK_ID_32(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));
				blk_pter -= offset;
				PUT_BLK_ID_32((recBase + SIZEOF(rec_hdr) + key_len), blk_pter);
				if (0 == blk_pter)
				{	/* if record tagged as for a killed global anticipate its later drop */
					new_blk_sz -= rec_sz;
					tot_kill_byte_cnt += rec_sz;
				} else	/* add the size increment for the larger pointers */
					new_blk_sz += (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
			}
			((blk_hdr_ptr_t)blkBase)->bver = dirHist.cr->ondsk_blkver = GDSV6p;/* ver to cr in case split updates */
		}	/* done with offset adjustment */
	}	/* done with level 0 block offset adjustment */
	while (recBase < blkEnd)	/* level 0 blocks may skip the loop below unless they reprocess due to a split */
	{	/* iterate through index block counting records */
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase);
		if (cdb_sc_starrecord == status)
		{
			assert((((rec_hdr_ptr_t)recBase)->rsiz + recBase == blkEnd) && (0 != level));
			memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
			key_len = 0;
		} else if (cdb_sc_normal != status)
		{	/* failed to parse record */
			assert(cdb_sc_normal == status);
			return status;
		}
		READ_BLK_ID(long_blk_id, &blk_pter, SIZEOF(rec_hdr) + recBase + key_len);
		assert((cs_addrs->ti->total_blks > blk_pter) && (0 < blk_pter));
		status = upgrade_dir_tree(blk_pter, offset, blk_size, reg);
		if (0 > status)
		{	/* negative status means a split - readjust by reprocessing the split tree */
			if (-1 != status)
				return ++status;
			if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
			{	/* Failed to read the indicated block */
				status = (enum cdb_sc)rdfail_detail;
				assert(cdb_sc_normal == (enum cdb_sc)status);
				t_abort(gv_cur_region, cs_addrs);				/* do crit and other cleanup */
				return status;
			}
			assert(blkBase == dirHist.buffaddr);
			new_blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
			blkEnd = blkBase + new_blk_sz;
			dirHist.level = level = ((blk_hdr_ptr_t)blkBase)->levl;
			assert(long_blk_id == IS_64_BLK_ID(blkBase));
			assert(blk_ver == ((blk_hdr_ptr_t)blkBase)->bver);
			recBase = blkBase + SIZEOF(blk_hdr);
			DBGUPGRADE(util_out_print("reprocessing level !UL directory block @x!@XQ", TRUE, level, &curr_blk));
			continue;
		}
		if (cdb_sc_normal != status)
		{	/* searching from this record resulted in an error so stop processing */
			assert(cdb_sc_normal == status);
			return status;
		}
		if (GDSV7m == blk_ver)
			return cdb_sc_normal;
		rec_sz = ((rec_hdr_ptr_t)recBase)->rsiz;
		recBase += rec_sz;								/* ready for the next record */
		new_blk_sz += (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
	}
	recBase = blkBase + SIZEOF(blk_hdr);
	assert(!update_trans && !need_kip_incr);
	update_trans = UPDTRNS_DB_UPDATED_MASK;
	t_begin_crit(ERR_MUNOUPGRD);
	gvnh_reg = NULL;	/* to silence [-Wuninitialized] warning */
	status = find_dt_entry_for_blk(&dirHist, blkBase, recBase, &gvname, gvnh_reg);
	if (cdb_sc_normal != status)
	{
		assert(cdb_sc_normal == status);
		t_abort(gv_cur_region, cs_addrs);
		return status;
	}
	dirHist = gv_target->hist.h[level];
	if (0 < (space_need = new_blk_sz - blk_size))						/* WARNING assignment */
	{	/* insufficient room? */
		space_need += SIZEOF(blk_hdr) + ((rec_hdr_ptr_t)recBase)->rsiz + (blk_size >> 3);
		assert((blk_size >> 1) > space_need);						/* paranoid check */
		DBGUPGRADE(util_out_print("splitting level !UL directory block @x!@XQ", TRUE, level, &curr_blk));
		split_blks_added = split_levels_added = 0;
		mu_reorg_process = TRUE;
		if (cdb_sc_normal != (status = mu_split(level, space_need, space_need, &split_blks_added, &split_levels_added)))
		{	/* split failed; WARNING: assignment above */
			assert(cdb_sc_normal == status);
			t_abort(gv_cur_region, cs_addrs);					/* do crit and other cleanup */
			return status;
		}
		if ((trans_num)0 == t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED)) /* t_end:508 bm_getfree ??? 6 */
		{
			status = t_fail_hist[t_tries - 1];
			assert(cdb_sc_normal == status);
			t_abort(gv_cur_region, cs_addrs);					/* do crit and other cleanup */
			return status;
		}
		mu_reorg_process = FALSE;
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		new_blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
		assert((new_blk_sz <= blk_size) && split_blks_added);
		tot_splt_cnt += split_blks_added;
		tot_levl_cnt += split_levels_added;
		split_blks_added -= split_levels_added;
		if (0 < split_blks_added)
			return -split_blks_added;					/* force reprocessing after the split */
		assert(FALSE);
	}
	assert(new_blk_sz < blk_size);
	/* Finally upgrade the block */
	if (SIZEOF(blk_hdr) == new_blk_sz)
	{	/* block became empty  TODO: ??? find its parent and remove it */
		assert(0 == level);
		assert(FALSE);
	}
	dirHist.blk_num = curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{	/* Failed to read the indicated block */
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == (enum cdb_sc)status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		return status;
	}
	assert(GDSV7m != blk_ver);
	dirHist.cr->ondsk_blkver = GDSV7m;							/* maintain as needed */
	assert((dirHist.buffaddr == blkBase) && (dirHist.level == level));
	CHECK_AND_RESET_UPDATE_ARRAY;
	BLK_INIT(bs_ptr, bs1);
	BLK_ADDR(v7bp, new_blk_sz, unsigned char);
	v7recBase = v7bp + SIZEOF(blk_hdr);
	((blk_hdr_ptr_t)v7bp)->bsiz = new_blk_sz;
	recBase = blkBase + SIZEOF(blk_hdr);
	for (rec_sz = v7_rec_sz = 0; recBase < blkEnd; recBase += rec_sz, v7recBase += v7_rec_sz)
	{	/* Update the recBase and v7recBase pointers to point to the next record */
		/* Parse the record to account for possible collation information after block pointer */
		/* Because blocks, including level 0, have pointers rather than application data, no spanning & bsiz not a worry */
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase);
		if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
		{
			assert((cdb_sc_normal == status) || (cdb_sc_starrecord == status));
			t_abort(gv_cur_region, cs_addrs);					/* do crit and other cleanup */
			return status;
		}
		GET_BLK_ID_32(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));
		if (0 == blk_pter)
		{
			assert(0 == level);
			tot_kill_block_cnt +=2;
			continue;	/* skip moving records pointing to killed globals as we previously deleted their trees */
		}
		v7_rec_sz = rec_sz + (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
		assert(blk_size > v7_rec_sz);
		/* Push the revised record into the update array */
		memcpy(v7recBase, recBase, SIZEOF(rec_hdr) + key_len);
		assert((unsigned short)v7_rec_sz == v7_rec_sz);
		((rec_hdr_ptr_t)v7recBase)->rsiz = (unsigned short)v7_rec_sz;
		PUT_BLK_ID_64((v7recBase + SIZEOF(rec_hdr) + key_len), blk_pter);
		if (rec_sz > (SIZEOF(rec_hdr) + key_len + SIZEOF_BLK_ID(BLKID_32)))
		{	/* This record contained collation information that also has to be copied */
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
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		return status;
	}
	t_write(&dirHist, (unsigned char *)bs1, 0, 0, level, TRUE, TRUE, GDS_WRITE_KILLTN);
	inctn_opcode = inctn_mu_reorg;
	ret_tn = t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED);
	t_abort(gv_cur_region, cs_addrs);							/* do crit and other cleanup */
	assertpro(0 != ret_tn);							/* this is a fine fix you've gotten us into Ollie */
	tot_dt++;
#	ifdef DEBUG
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{	/* Failed to read the indicated block */
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == (enum cdb_sc)status);
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		return status;
	}
#	endif
	DBGUPGRADE(util_out_print("adjusted level !UL directory block @x!@XQ", TRUE, level, &curr_blk));
	return cdb_sc_normal; /* finished upgrading this block and also any leaf nodes descended from this block */
}
