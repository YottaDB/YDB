/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "util.h"
#include "gdsbml.h"
#include "bmm_find_free.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

error_def(ERR_DSEBLKRDFAIL);

void dse_f_free(void)
{
	block_id	blk, master_bit, mmap_hint, lmap_hint;
	block_cnt	total_blks;
	boolean_t	in_last_bmap, was_crit, was_hold_onto_crit;
	cache_rec_ptr_t	dummy_cr;
	char		util_buff[MAX_UTIL_LEN];
	int4		bplmap, dummy_int;
	int4		lmap_bit, nocrit_present, util_len;
	sm_uc_ptr_t	lmap_base;

	if (0 == cs_addrs->hdr->bplmap)
	{	util_out_print("Cannot perform free block search:  bplmap field of file header is zero.", TRUE);
		return;
	}
	bplmap = cs_addrs->hdr->bplmap;
	if (BADDSEBLK == (blk = dse_getblk("HINT", DSEBMLOK, DSEBLKNOCUR)))		/* WARNING: assignment */
		return;
	mmap_hint = blk / bplmap;
	master_bit = bmm_find_free(mmap_hint, cs_addrs->bmm,
			(cs_addrs->ti->total_blks + bplmap - 1)/ bplmap);
	if (-1 == master_bit)
	{	util_out_print("Error: database full.", TRUE);
		return;
	}
	in_last_bmap = (master_bit == (cs_addrs->ti->total_blks / bplmap));
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (!(lmap_base = t_qread(master_bit * bplmap, &dummy_int, &dummy_cr)))
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (master_bit == mmap_hint)
		lmap_hint = blk - ((blk / bplmap) * bplmap);
	else
		lmap_hint = 0;
	if (in_last_bmap)
		total_blks = (cs_addrs->ti->total_blks - master_bit);
	else
		total_blks = bplmap;
	lmap_bit = bml_find_free(lmap_hint, lmap_base + SIZEOF(blk_hdr), total_blks);
	if (-1 == lmap_bit)
	{	memcpy(util_buff, "Error: bit map in block ", 24);
		util_len = 24;
		util_len += i2hexl_nofill(master_bit * bplmap, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
		memcpy(&util_buff[util_len], " incorrectly marked free in master map.", 39);
		util_len += 39;
		util_buff[util_len] = 0;
		util_out_print(util_buff, TRUE);
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		return;
	}
	memcpy(util_buff, "!/Next free block is ", 21);
	util_len = 21;
	util_len += i2hexl_nofill((master_bit * bplmap) + lmap_bit, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
	memcpy(&util_buff[util_len], ".!/", 3);
	util_len += 3;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return;
}
