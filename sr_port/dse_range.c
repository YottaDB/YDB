/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.                                     *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_signal.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsdbver.h"
#include "cli.h"
#include "copy.h"
#include "min_max.h"
#include "util.h"
#include "dse.h"
#include "filestruct.h"

/* Include prototypes */
#include "t_qread.h"

GBLREF block_id		patch_find_blk, patch_path[MAX_BT_DEPTH + 1];
GBLREF boolean_t	patch_find_root_search;
GBLREF gd_region	*gv_cur_region;
GBLREF short int	patch_path_count;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF VSIG_ATOMIC_T	util_interrupt;

#ifdef DEBUG
GBLREF	block_id	ydb_skip_bml_num;
#endif

error_def(ERR_CTRLC);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEINVALBLKID);

void dse_range(void)
{
	block_id	from, to, blk, blk_child;
	boolean_t	busy_matters, free, got_lonely_star, index, lost, low, star, up, was_crit, was_hold_onto_crit = FALSE,
			long_blk_id;
	cache_rec_ptr_t	dummy_cr;
	char		level, lower[MAX_KEY_SZ + 1], targ_key[MAX_KEY_SZ + 1], upper[MAX_KEY_SZ + 1];
	int		cnt, dummy, lower_len, upper_len;
	int4		dummy_int, nocrit_present;
	long		blk_id_size;
	short int	rsize, size, size1;
	sm_uc_ptr_t	bp, b_top, key_bot, key_top, key_top1, rp, r_top;

	DSE_DB_IS_TOO_OLD(cs_addrs, cs_data, gv_cur_region);
	if (CLI_PRESENT == cli_present("FROM"))
	{
		if (BADDSEBLK == (from = dse_getblk("FROM", DSEBMLOK, DSEBLKNOCUR)))	/* WARNING: assignment */
			return;
	} else
		from = 1;
	if (CLI_PRESENT == cli_present("TO"))
	{
		if (BADDSEBLK == (to = dse_getblk("TO", DSEBMLOK, DSEBLKNOCUR)))	/* WARNING: assignment */
			return;
	} else
		to = cs_addrs->ti->total_blks - 1;
	if (low = (CLI_PRESENT == cli_present("LOWER")))				/* WARNING: assignment */
	{
		if (!dse_getki(&lower[0], &lower_len, LIT_AND_LEN("LOWER")))
			return;
	}
	if (up = (CLI_PRESENT == cli_present("UPPER")))					/* WARNING: assignment */
	{
		if (!dse_getki(&upper[0], &upper_len, LIT_AND_LEN("UPPER")))
			return;
	}
	star = (CLI_PRESENT == cli_present("STAR"));
	if (!low && !up && !star)
	{
		util_out_print("Must specify star, or a lower or upper key limit.", TRUE);
		return;
	}
	index = (CLI_PRESENT == cli_present("INDEX"));
	lost = (CLI_PRESENT == cli_present("LOST"));
	dummy = cli_present("BUSY");
	if (CLI_PRESENT == dummy)
	{
		busy_matters = TRUE;
		free = FALSE;
	} else if (CLI_NEGATED == dummy)
		busy_matters = free = TRUE;
	else
		busy_matters = free = FALSE;
	patch_path[0] = get_dir_root();
	cnt = 0;
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	for (blk = from; blk <= to ;blk++)
	{
#		ifdef DEBUG
		if ((0 != ydb_skip_bml_num) && (BLKS_PER_LMAP <= blk) && (blk < ydb_skip_bml_num))
		{
			blk = ydb_skip_bml_num - 1;
			continue;
		}
#		endif
		if (util_interrupt)
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
			break;
		}
		if (!(blk % cs_addrs->hdr->bplmap))
			continue;
		if (!(bp = t_qread(blk, &dummy_int, &dummy_cr)))
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);
		if (((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER)
		{
#			ifdef BLK_NUM_64BIT
			long_blk_id = TRUE;
			blk_id_size = SIZEOF(block_id_64);
#			else
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#			endif
		} else
		{
			long_blk_id = FALSE;
			blk_id_size = SIZEOF(block_id_32);
		}
		level = ((blk_hdr_ptr_t)bp)->levl;
		if (index && (0 == level))
			continue;
		if (busy_matters && (free != dse_is_blk_free(blk, &dummy_int, &dummy_cr)))
			continue;
		if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
			b_top = bp + cs_addrs->hdr->blk_size;
		else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
			b_top = bp + SIZEOF(blk_hdr);
		else
			b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
		rp = bp + SIZEOF(blk_hdr);
		GET_SHORT(rsize, &((rec_hdr_ptr_t) rp)->rsiz);
		if (rsize < SIZEOF(rec_hdr))
			r_top = rp + SIZEOF(rec_hdr);
		else
			r_top = rp + rsize;
		if (r_top >= b_top)
			r_top = b_top;
		got_lonely_star = FALSE;
		if (((blk_hdr_ptr_t)bp)->levl)
		{
			key_top = r_top - blk_id_size;
			if (star && (r_top == b_top))
				got_lonely_star = TRUE;
		} else
		{
			if (!up && !low)
				continue;
			for (key_top = rp + SIZEOF(rec_hdr); key_top < r_top ; )
			{
				if (!*key_top++ && (key_top < r_top) && !*key_top++)
					break;
			}
		}
		if (!got_lonely_star)
		{
			key_bot = rp + SIZEOF(rec_hdr);
			size = key_top - key_bot;
			if (size <= 0)
				continue;
			if (size > SIZEOF(targ_key))
				size = SIZEOF(targ_key);
			if (lost)
			{
				for (key_top1 = rp + SIZEOF(rec_hdr); key_top1 < r_top ; )
					if (!*key_top1++)
						break;
				size1 = key_top1 - rp - SIZEOF(rec_hdr);
				if (size1 > SIZEOF(targ_key))
					size1 = SIZEOF(targ_key);
				patch_find_root_search = TRUE;
				patch_path_count = 1;
				patch_find_blk = blk;
				if (dse_is_blk_in(rp, r_top, size1))
					continue;
			}
			if (low && memcmp(lower, key_bot, MIN(lower_len, size)) > 0)
				continue;
			if (up && memcmp(upper, key_bot, MIN(upper_len, size)) < 0)
				continue;
		} else
		{
			if (lost)
			{
				if (long_blk_id)
				{
#					ifdef BLK_NUM_64BIT
					GET_BLK_ID_64(blk_child, key_top);
#					else
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
							nocrit_present, cs_addrs, gv_cur_region);
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				} else
					GET_BLK_ID_32(blk_child, key_top);
				if (!(bp = t_qread(blk_child, &dummy_int, &dummy_cr)))
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);
				if (((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER)
				{
#					ifndef BLK_NUM_64BIT
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
							nocrit_present, cs_addrs, gv_cur_region);
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				}
				if (((blk_hdr_ptr_t)bp)->bsiz > cs_addrs->hdr->blk_size)
					b_top = bp + cs_addrs->hdr->blk_size;
				else if (((blk_hdr_ptr_t)bp)->bsiz < SIZEOF(blk_hdr))
					b_top = bp + SIZEOF(blk_hdr);
				else
					b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
				rp = bp + SIZEOF(blk_hdr);
				GET_SHORT(rsize, &((rec_hdr_ptr_t) rp)->rsiz);
				if (rsize < SIZEOF(rec_hdr))
					r_top = rp + SIZEOF(rec_hdr);
				else
					r_top = rp + rsize;
				if (r_top >= b_top)
					r_top = b_top;
				for (key_top1 = rp + SIZEOF(rec_hdr); key_top1 < r_top ; )
					if (!*key_top1++)
						break;
				size1 = key_top1 - rp - SIZEOF(rec_hdr);
				if (size1 > 0)
				{
					if (size1 > SIZEOF(targ_key))
						size1 = SIZEOF(targ_key);
					patch_find_root_search = TRUE;
					patch_path_count = 1;
					patch_find_blk = blk;
					if (dse_is_blk_in(rp, r_top, size1))
						continue;
				}
			}
		}
		if (!cnt++)
			util_out_print("!/Blocks in the specified key range:", TRUE);
		util_out_print("Block:  0x!16@XQ Level: !2UL", TRUE, &blk, level);
	}
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (cnt)
		util_out_print("Found !UL blocks", TRUE, cnt);
	else
		util_out_print("None found.", TRUE);
	return;
}
