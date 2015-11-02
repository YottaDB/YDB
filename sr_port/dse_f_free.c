/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;

error_def(ERR_DSEBLKRDFAIL);

#define MAX_UTIL_LEN 80

void dse_f_free(void)
{
	block_id	blk;
	bool		in_last_bmap;
	char		util_buff[MAX_UTIL_LEN];
	sm_uc_ptr_t	lmap_base;
	int4		bplmap, total_blks;
	int4		util_len, master_bit, lmap_bit, hint_over_bplmap, hint_mod_bplmap;
	boolean_t	was_crit, was_hold_onto_crit;
	int4		dummy_int, nocrit_present;
	cache_rec_ptr_t	dummy_cr;

	if (cs_addrs->hdr->bplmap == 0)
	{	util_out_print("Cannot perform free block search:  bplmap field of file header is zero.", TRUE);
		return;
	}
	bplmap = cs_addrs->hdr->bplmap;

	if(!cli_get_hex("HINT", (uint4 *)&blk))
		return;
	if (blk < 0 || blk >= cs_addrs->ti->total_blks || (blk / bplmap * bplmap == blk))
	{	util_out_print("Error: invalid block number.", TRUE);
		return;
	}
	hint_over_bplmap = blk / bplmap;
	master_bit = bmm_find_free(hint_over_bplmap, cs_addrs->bmm,
			(cs_addrs->ti->total_blks + bplmap - 1)/ bplmap);
	if (master_bit == -1)
	{	util_out_print("Error: database full.", TRUE);
		return;
	}
	in_last_bmap = (master_bit == (cs_addrs->ti->total_blks / bplmap));
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if(!(lmap_base = t_qread(master_bit * bplmap, &dummy_int, &dummy_cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (master_bit == hint_over_bplmap)
		hint_mod_bplmap = blk - blk / bplmap * bplmap;
	else
		hint_mod_bplmap = 0;
	if (in_last_bmap)
		total_blks = (cs_addrs->ti->total_blks - master_bit);
	else
		total_blks = bplmap;
	lmap_bit = bml_find_free(hint_mod_bplmap, lmap_base + SIZEOF(blk_hdr), total_blks);
	if (lmap_bit == -1)
	{	memcpy(util_buff, "Error: bit map in block ", 24);
		util_len = 24;
		util_len += i2hex_nofill(master_bit * bplmap, (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], " incorrectly marked free in master map.", 39);
		util_len += 39;
		util_buff[util_len] = 0;
		util_out_print(util_buff, TRUE);
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		return;
	}
	memcpy(util_buff, "!/Next free block is ", 21);
	util_len = 21;
	util_len += i2hex_nofill(master_bit * bplmap + lmap_bit, (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], ".!/", 3);
	util_len += 3;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return;
}
