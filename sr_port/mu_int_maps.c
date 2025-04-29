/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsdbver.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "copy.h"
#include "mu_int_maps.h"
#include "util.h"
#include "mupint.h"
#include "gtmmsg.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "gtm_time.h"
#include "gtm_c_stack_trace.h"

/* Include prototypes */
#include "bit_set.h"

GBLREF	uint4			mu_int_offset[];
GBLREF	uint4			mu_int_errknt;
GBLREF	unsigned char		*mu_int_locals;
GBLREF	unsigned char		*mu_int_master;
GBLREF	sgmnt_data		mu_int_data;
GBLREF	block_id		mu_int_path[];
GBLREF	boolean_t		tn_reset_this_reg;
GBLREF	int			mu_int_plen;
GBLREF	block_id		mu_int_blks_to_upgrd;
GBLREF	int			disp_map_errors;
GBLREF	int			mu_map_errs;
GBLREF	int			disp_trans_errors;
GBLREF	int			trans_errors;
GBLREF	boolean_t		debug_mupip;
GBLREF	sgmnt_data		*cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;

GBLREF trans_num	largest_tn;

error_def(ERR_DBREADBM);
error_def(ERR_DBLVLINC);
error_def(ERR_DBMBSIZMN);
error_def(ERR_DBMBSIZMX);
error_def(ERR_DBMBTNSIZMX);
error_def(ERR_DBLOCMBINC);
error_def(ERR_DBBFSTAT);
error_def(ERR_DBMBPFLINT);
error_def(ERR_DBMBPFLDLBM);
error_def(ERR_DBMBPFRDLBM);
error_def(ERR_DBMBPFRINT);
error_def(ERR_DBMBPFLDIS);
error_def(ERR_DBMBPINCFL);
error_def(ERR_DBMRKFREE);
error_def(ERR_DBMRKBUSY);
error_def(ERR_DBMBMINCFRE);
error_def(ERR_DBTN);

void mu_int_maps(void)
{
	unsigned char	*local;
	char		time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro */
	uchar_ptr_t	blk_base, buff_blk_base, free_blk_base;
	boolean_t	wait_on_kip, agree, disk_full, local_full, master_full;
	block_id	maps, mapsize, mcnt, lcnt, bcnt;
	unsigned int	level;
	uint_ptr_t	dskmap_p;
	uint4		dskmap, lfree, *lmap, map_blk_size, crit_counter;
	block_id	blkno, last_bmp;
	enum db_ver	ondsk_blkver;
	trans_num map_tn;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Before going into mu_int_maps, check if there are any KIPs and wait for them */
	crit_counter = 1;
	wait_on_kip = (cs_data && cs_data->kill_in_prog) || mu_int_data.kill_in_prog;
	if (wait_on_kip)
	{
		GET_CUR_TIME(time_str);
		util_out_print("!/MUPIP INFO: mu_int_maps: !AD : Start kill-in-prog wait.", TRUE,
			CTIME_BEFORE_NL, time_str);
	}
#	ifdef DEBUG
	if (WBTEST_ENABLED(WBTEST_INTEG_RTS_ERR))
		RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_ERROR(ERR_TEXT), 2,
				RTS_ERROR_TEXT("White Box integ rts_error"));
#	endif
	while (((cs_data && cs_data->kill_in_prog) || mu_int_data.kill_in_prog) && (MAX_CRIT_TRY > crit_counter++))
	{
		if (cs_addrs)
			GET_C_STACK_FOR_KIP(cs_addrs->nl->kip_pid_array, crit_counter, MAX_CRIT_TRY, 1, MAX_KIP_PID_SLOTS);
		wcs_sleep(crit_counter);
	}
	if (wait_on_kip)
	{
		GET_CUR_TIME(time_str);
		util_out_print("!/MUPIP INFO: mu_int_maps: !AD : Done with kill-in-prog wait.", TRUE,
			CTIME_BEFORE_NL, time_str);
	}
	mu_int_offset[0] = 0;
	maps = (mu_int_data.trans_hist.total_blks + mu_int_data.bplmap - 1) / mu_int_data.bplmap;
	local = mu_int_locals;
	map_blk_size = BM_SIZE(mu_int_data.bplmap);
	last_bmp = ((mu_int_data.trans_hist.total_blks / mu_int_data.bplmap) * mu_int_data.bplmap);
	mapsize = mu_int_data.bplmap;
	for (mcnt = 0;  mcnt < maps;  mcnt++, local += BM_MINUS_BLKHDR_SIZE(mapsize))
	{
		assert(mapsize == mu_int_data.bplmap);
		blkno = mcnt * mu_int_data.bplmap;
		bml_busy(0, mu_int_locals + ((blkno * BML_BITS_PER_BLK) / BITS_PER_UCHAR));
		blk_base = mu_int_read(blkno, &ondsk_blkver, &free_blk_base);
		if (!blk_base)
		{
			mu_int_path[0] = blkno;
			mu_int_plen = 1;
			mu_int_err(ERR_DBREADBM, 0, 0, 0, 0, 0, 0, LCL_MAP_LEVL);
			continue;
		}
		if (LCL_MAP_LEVL != (level = (unsigned int)((blk_hdr_ptr_t)blk_base)->levl))
		{
			mu_int_path[0] = blkno;
			mu_int_plen = 1;
			mu_int_err(ERR_DBLVLINC, 0, 0, 0, 0, 0, 0, level);
		}
		if (((blk_hdr_ptr_t)blk_base)->bsiz < map_blk_size)
		{
			mu_int_path[0] = blkno;
			mu_int_plen = 1;
			mu_int_err(ERR_DBMBSIZMN, 0, 0, 0, 0, 0, 0, level);
			continue;
		}
		if (((blk_hdr_ptr_t)blk_base)->bsiz > map_blk_size)
		{
			mu_int_path[0] = blkno;
			mu_int_plen = 1;
			mu_int_err(ERR_DBMBSIZMX, 0, 0, 0, 0, 0, 0, level);
			continue;
		}
		/* here we might maintain mu_int_blks_to_upgrd for local bit maps, but none is needed for V6->V7 */
		if (tn_reset_this_reg)
		{
			((blk_hdr_ptr_t)blk_base)->tn = 0;
			mu_int_write(blkno, blk_base);
		}
		map_tn = ((blk_hdr_ptr_t)blk_base)->tn;
		if (map_tn >= mu_int_data.trans_hist.curr_tn)
		{
			if (trans_errors < disp_trans_errors)
			{
				mu_int_path[0] = blkno;
				mu_int_plen = 1;
				mu_int_err(ERR_DBMBTNSIZMX, 0, 0, 0, 0, 0, 0, level);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_DBTN, 1, &map_tn);
			} else
				mu_int_errknt++;
			trans_errors++;
			if (map_tn > largest_tn)
				largest_tn = map_tn;
		}
		master_full = !bit_set(mcnt, mu_int_master);
		if (last_bmp == blkno)
			mapsize = (mu_int_data.trans_hist.total_blks - blkno);
		disk_full = (NO_FREE_SPACE == bml_find_free(0, blk_base + SIZEOF(blk_hdr), mapsize));
		agree = TRUE;
		if (TREF(instance_frozen_crit_skipped))
		{
			/* Do a pre-check for comparison of bitmap on disk with that prepared by integ.
			 * If there is a mismatch, then read the block from buffer.
			 */
			if (debug_mupip)
				util_out_print("!/MUPIP INFO: mu_int_maps: 1st round of pre-check with block from Disk", TRUE);
			for (lcnt = 0, dskmap_p = (uint_ptr_t)(blk_base + SIZEOF(blk_hdr)), lmap = (uint4 *)local;
				lcnt < mapsize;
				lcnt += SIZEOF(int4) * BITS_PER_UCHAR / BML_BITS_PER_BLK,
				dskmap_p++, lmap++)  /* # of bits/ bits per blk */
			{
				GET_LONG(dskmap, dskmap_p);

				if ((dskmap & SIXTEEN_BLKS_FREE) != *lmap)
				{
					if (debug_mupip)
						util_out_print(
							"!/MUPIP INFO: mu_int_maps: 1st precheck failed. retrying block in buffer",
								TRUE);
					if (free_blk_base)
						free(free_blk_base);
					free_blk_base = NULL;
					buff_blk_base = mu_int_read_buffer(blkno, &ondsk_blkver, &free_blk_base);
					break;
				}
			}
			/* Do the precheck once more if the block is read from buffer. Determine which block to use further. */
			if ((lcnt < mapsize) && buff_blk_base)
			{
				if (debug_mupip)
					util_out_print(
						"!/MUPIP INFO: mu_int_maps: Doing 2nd round of pre-check with block from Buffer",
							TRUE);
				for (lcnt = 0, dskmap_p = (uint_ptr_t)(buff_blk_base + SIZEOF(blk_hdr)), lmap = (uint4 *)local;
					lcnt < mapsize;
					lcnt += SIZEOF(int4) * BITS_PER_UCHAR / BML_BITS_PER_BLK,
					dskmap_p++, lmap++)  /* # of bits/ bits per blk */
				{
					GET_LONG(dskmap, dskmap_p);

					if ((dskmap & SIXTEEN_BLKS_FREE) != *lmap)
					{
						if (debug_mupip)
							util_out_print(
							"!/MUPIP INFO: mu_int_maps: precheck with Block from buffer also failed.",
								TRUE);
						break;
					}
				}
				if (lcnt >= mapsize)
				{
					if (debug_mupip)
						util_out_print("!/MUPIP INFO: mu_int_maps: Block from buffer succeeds precheck",
										TRUE);
					blk_base = buff_blk_base;
				}
			}
		}
		/* Continue with the block chosen above (always in blk_base) */
		for (lcnt = 0, dskmap_p = (uint_ptr_t)(blk_base + SIZEOF(blk_hdr)), lmap = (uint4 *)local;
			lcnt < mapsize;
			lcnt += SIZEOF(int4) * BITS_PER_UCHAR / BML_BITS_PER_BLK,
			dskmap_p++, lmap++)  /* # of bits/ bits per blk */
		{
			GET_LONG(dskmap, dskmap_p);

			/* do a quick check to see if there is anything wrong with the
			   bitmaps entries that fit into an int4.

			   We need to check that the free bit matches the in-memory
			   copy and that there are no illegal combinations.

			   There are four combinations per block
			   (00 = busy, 01 = free, 10 = unused (invalid bitmap value),
			    11 = free block which was previously used).

			   We use the following calculation to determine if there are
			   any illegal combinations within the current int4.

			        00011011	take original value from bitmap on disk.
			    and 01010101        mask off "reused" bit.
			       ----------
			        00010001	= free blocks.
			    xor 01010101	toggle free bit
			       ----------
			        01000100	= busy blocks
				<< 1
			       ----------
			        10001000	= mask checking the "reused" bit  for the busy blocks
			    and 00011011	original value from bitmap on disk.
			       ----------
			        00001000	non-zero indicates an illegal combination somewhere
						in the int4.
			 */
			if ((dskmap & SIXTEEN_BLKS_FREE) != *lmap
				|| ((((dskmap & SIXTEEN_BLKS_FREE) ^ SIXTEEN_BLKS_FREE) << 1) & dskmap))
			{
				if (agree)
				{
					agree = FALSE;
					mu_int_path[0] = blkno;
					mu_int_plen = 1;
					if (mu_map_errs < disp_map_errors)
					{
						if (TREF(instance_frozen_crit_skipped))
							util_out_print(
	"Instance Frozen. Please wait for the freeze to be lifted before verifying and, if necessary, fixing the following errors.",
								 TRUE);
						mu_int_err(ERR_DBLOCMBINC, 0, 0, 0, 0, 0, 0, level);
					} else
						mu_int_errknt++;
				}
				for (bcnt = 0;  bcnt < SIZEOF(int4) * BITS_PER_UCHAR / BML_BITS_PER_BLK;  bcnt++)
				{
					if (!(mu_int_isvalid_mask[bcnt] ^ (dskmap & mu_int_mask[bcnt])))
					{
						mu_int_path[0] = blkno + lcnt + bcnt;
						mu_int_plen = 1;
						mu_int_err(ERR_DBBFSTAT,
								0, 0, 0, 0, 0, 0, level);
					}
					else  if ((lfree = mu_int_isfree_mask[bcnt] & *(lmap))
							^ (mu_int_isfree_mask[bcnt] & dskmap))
					{
						mu_int_path[0] = blkno + lcnt + bcnt;
						mu_int_plen = 1;
						/* for the following two mu_int_err(), we should actually be calculating the
						 * actual level of the mu_int_path[0]. But this would need a read() of the block,
						 * which might slow down the process. We should consider this however at a
						 * later time.  */
						if (!lfree)
							mu_int_err(ERR_DBMRKFREE, 0, 0, 0, 0, 0, 0, LCL_MAP_LEVL);
						else  if (mu_map_errs < disp_map_errors)
						{
							mu_int_err(ERR_DBMRKBUSY, 0, 0, 0, 0, 0, 0, LCL_MAP_LEVL);
							mu_map_errs++;
						} else
						{
							mu_int_errknt++;
							mu_map_errs++;
						}
					}
				}
			}
		}
		if (!agree)
		{
			local_full = (NO_FREE_SPACE == bml_find_free(0, local, mapsize));
			if (local_full || disk_full)
			{
				mu_int_path[0] = blkno;
				mu_int_plen = 1;
				if (mu_map_errs < disp_map_errors)
				{
					mu_int_err(master_full ?
						(local_full ? ERR_DBMBPFLINT
							: ERR_DBMBPFLDLBM)
						: (local_full ?
							ERR_DBMBPFRDLBM
							: ERR_DBMBPFRINT),
					0, 0, 0, 0, 0, 0, level);
				} else
					mu_int_errknt++;
			} else  if (master_full)
			{
				if (mu_map_errs < disp_map_errors)
				{
					mu_int_path[0] = blkno;
					mu_int_plen = 1;
					mu_int_err( ERR_DBMBPFLDIS,
						0, 0, 0, 0, 0, 0, level);
				} else
					mu_int_errknt++;
			}
		} else  if (disk_full ^ master_full)
		{
			if (mu_map_errs < disp_map_errors)
			{
				mu_int_path[0] = blkno;
				mu_int_plen = 1;
				mu_int_err(master_full ? ERR_DBMBPINCFL
					: ERR_DBMBMINCFRE,
						0, 0, 0, 0, 0, 0, level);
			} else
				mu_int_errknt++;
		}
		if (free_blk_base)
			free(free_blk_base);
	}
	if (mu_map_errs >= disp_map_errors)
	{
		util_out_print("Maximum number of incorrectly busy errors to display:  !UL, has been exceeded", TRUE,
			disp_map_errors);
		util_out_print("!UL incorrectly busy errors encountered", TRUE,	mu_map_errs);
	}
	return;
}
