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

#include <signal.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "copy.h"
#include "min_max.h"
#include "util.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"

#define MAX_UTIL_LEN 20

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF VSIG_ATOMIC_T	util_interrupt;
GBLREF block_id		patch_find_blk;
GBLREF block_id		patch_path[MAX_BT_DEPTH + 1];
GBLREF short int	patch_path_count;
GBLREF bool		patch_find_root_search;

error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_CTRLC);

void dse_range(void)
{
    char		lower[MAX_KEY_SZ + 1], targ_key[MAX_KEY_SZ + 1], upper[MAX_KEY_SZ + 1], util_buff[MAX_UTIL_LEN];
    block_id		from, to, blk, blk_child;
    sm_uc_ptr_t		bp, b_top, key_bot, key_top, key_top1, rp, r_top;
    char		level;
    int4		dummy_int, nocrit_present;
    cache_rec_ptr_t	dummy_cr;
    short int		rsize, size, size1;
    int			cnt, dummy, lower_len, util_len, upper_len;
    boolean_t		busy_matters, free, got_lonely_star, index, low, lost, star, up, was_crit, was_hold_onto_crit;

    if (cli_present("FROM") == CLI_PRESENT)
    {
	if (!cli_get_hex("FROM", (uint4 *)&from))
	    return;
	if (from < 0 || from > cs_addrs->ti->total_blks
	    || !(from % cs_addrs->hdr->bplmap))
	{
	    util_out_print("Error: invalid block number.", TRUE);
	    return;
	}
    }
    else
	from = 1;
    if (cli_present("TO") == CLI_PRESENT)
    {
	if(!cli_get_hex("TO", (uint4 *)&to))
	    return;
	if (to < 0 || to > cs_addrs->ti->total_blks
	    || !(to % cs_addrs->hdr->bplmap))
	{
	    util_out_print("Error: invalid block number.", TRUE);
	    return;
	}
    }
    else
	to = cs_addrs->ti->total_blks - 1;
    if (low = (cli_present("LOWER") == CLI_PRESENT))
    {
	if (!dse_getki(&lower[0], &lower_len, LIT_AND_LEN("LOWER")))
	    return;
    }
    if (up = (cli_present("UPPER") == CLI_PRESENT))
    {
	if (!dse_getki(&upper[0], &upper_len, LIT_AND_LEN("UPPER")))
	    return;
    }
    star = (cli_present("STAR") == CLI_PRESENT);
    if (!low && !up && !star)
    {
	util_out_print("Must specify star, or a lower or upper key limit.", TRUE);
	return;
    }
    index = (cli_present("INDEX") == CLI_PRESENT);
    lost = (cli_present("LOST") == CLI_PRESENT);
    dummy = cli_present("BUSY");
    if (dummy == CLI_PRESENT)
    {
	busy_matters = TRUE;
	free = FALSE;
    }
    else if (dummy == CLI_NEGATED)
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
	if (util_interrupt)
	{
	    DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	    rts_error(VARLSTCNT(1) ERR_CTRLC);
	    break;
	}
	if (!(blk % cs_addrs->hdr->bplmap))
	    continue;
	if (!(bp = t_qread(blk, &dummy_int, &dummy_cr)))
	    rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	level = ((blk_hdr_ptr_t)bp)->levl;
	if (index && (level == 0))
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
	if (((blk_hdr_ptr_t) bp)->levl)
	{
	    key_top = r_top - SIZEOF(block_id);
	    if (star && (r_top == b_top))
		got_lonely_star = TRUE;
	} else
	{
	    if (!up && !low)
		continue;
	    for (key_top = rp + SIZEOF(rec_hdr); key_top < r_top ; )
		if (!*key_top++ && !*key_top++)
		    break;
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
		got_lonely_star = FALSE;
		if (lost)
		{
			blk_child = *(block_id_ptr_t)key_top;
			if (!(bp = t_qread(blk_child, &dummy_int, &dummy_cr)))
			    rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
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
			if (((blk_hdr_ptr_t) bp)->levl)
			    key_top = r_top - SIZEOF(block_id);
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
	util_out_print("Block:  !8XL Level: !2UL", TRUE, blk, level);
    }
    DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
    if (cnt)
	util_out_print("Found !UL blocks", TRUE, cnt);
    else
	util_out_print("None found.", TRUE);
    return;
}
