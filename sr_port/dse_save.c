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

#include "gtm_string.h"
#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dse.h"
#include "cli.h"
#include "util.h"

/* Include prototypes */
#include "t_qread.h"

#define MAX_UTIL_LEN 80

GBLDEF save_strct	patch_save_set[PATCH_SAVE_SIZE];
GBLDEF unsigned short	patch_save_count = 0;

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF block_id		patch_curr_blk;

error_def(ERR_DSEBLKRDFAIL);

void dse_save(void)
{
	block_id	blk;
	unsigned	i, j, util_len;
	unsigned short	buff_len;
	boolean_t	was_block, was_crit, was_hold_onto_crit;
	char		buff[100], *ptr, util_buff[MAX_UTIL_LEN];
	sm_uc_ptr_t	bp;
	int4		dummy_int, nocrit_present;
	cache_rec_ptr_t dummy_cr;

	memset(util_buff, 0, MAX_UTIL_LEN);

	if (was_block = (cli_present("BLOCK") == CLI_PRESENT))
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
			return;
		if (blk < 0 || blk >= cs_addrs->ti->total_blks)
		{
			util_out_print("Error: invalid block number.", TRUE);
			return;
		}
		patch_curr_blk = blk;
	} else
		blk = patch_curr_blk;
	if (cli_present("LIST") == CLI_PRESENT)
	{
		if (was_block)
		{
			util_len = SIZEOF("!/Saved versions of block ");
			memcpy(util_buff, "!/Saved versions of block ", util_len);
			util_len += i2hex_nofill(blk, (uchar_ptr_t)&util_buff[util_len-1], 8);
			util_buff[util_len-1] = 0;
			util_out_print(util_buff, TRUE);
			for (i = j = 0;  i < patch_save_count;  i++)
				if (patch_save_set[i].blk == blk)
				{
					j++;

					if (*patch_save_set[i].comment)
						util_out_print("Version !UL  Region !AD  Comment: !AD!/", TRUE,
							patch_save_set[i].ver, REG_LEN_STR(patch_save_set[i].region),
							LEN_AND_STR(patch_save_set[i].comment));

					else
						util_out_print("Version !UL  Region !AD!/", TRUE, patch_save_set[i].ver,
							REG_LEN_STR(patch_save_set[i].region));
				}
			if (!j)
				util_out_print("None.!/", TRUE);
			return;
		}
		util_out_print("!/Save history:!/", TRUE);
		for (i = j = 0;  i < patch_save_count;  i++)
		{
			util_len = SIZEOF("Block ");
			memcpy(util_buff, "Block ", util_len);
			util_len += i2hex_nofill(patch_save_set[i].blk, (uchar_ptr_t)&util_buff[util_len-1], 8);
			util_buff[util_len-1] = 0;
			util_out_print(util_buff, TRUE);
			j++;
			if (*patch_save_set[i].comment)
			{
				util_out_print("Version !UL  Region !AD  Comment: !AD!/", TRUE,
					patch_save_set[i].ver, REG_LEN_STR(patch_save_set[i].region),
					LEN_AND_STR(patch_save_set[i].comment));

			} else
			{
				util_out_print("Version !UL  Region !AD!/", TRUE, patch_save_set[i].ver,
					REG_LEN_STR(patch_save_set[i].region));
			}
		}
		if (!j)
			util_out_print("  None.!/", TRUE);
		return;
	}
	j = 1;
	for (i = 0;  i < patch_save_count;  i++)
		if (patch_save_set[i].blk == blk && patch_save_set[i].region == gv_cur_region
			&& patch_save_set[i].ver >= j)
			j = patch_save_set[i].ver + 1;
	util_len = SIZEOF("!/Saving version !UL of block ");
	memcpy(util_buff, "!/Saving version !UL of block ", util_len);
	util_len += i2hex_nofill(blk, (uchar_ptr_t)&util_buff[util_len-1], 8);
	util_buff[util_len-1] = 0;
	util_out_print(util_buff, TRUE, j);
	patch_save_set[patch_save_count].ver = j;
	patch_save_set[patch_save_count].blk = blk;
	patch_save_set[patch_save_count].region = gv_cur_region;
	patch_save_set[patch_save_count].bp = (char *)malloc(cs_addrs->hdr->blk_size);
	if (blk >= cs_addrs->ti->total_blks)
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (!(bp = t_qread(blk, &dummy_int, &dummy_cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	memcpy(patch_save_set[patch_save_count].bp, bp, cs_addrs->hdr->blk_size);
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	buff_len = SIZEOF(buff);
	if ((cli_present("COMMENT") == CLI_PRESENT) && cli_get_str("COMMENT", buff, &buff_len))
	{
		ptr = &buff[buff_len];
		*ptr = 0;
		j = (unsigned int)(ptr - &buff[0] + 1);
		patch_save_set[patch_save_count].comment = (char *)malloc(j);
		memcpy(patch_save_set[patch_save_count].comment, &buff[0], j);
	} else
		patch_save_set[patch_save_count].comment = "";
	patch_save_count++;
	return;
}
