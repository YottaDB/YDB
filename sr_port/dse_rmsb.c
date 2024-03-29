/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dse.h"
#include "cli.h"
#include "util.h"

GBLREF block_id		patch_curr_blk;
GBLREF gd_region	*gv_cur_region;
GBLREF save_strct	patch_save_set[PATCH_SAVE_SIZE];
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF uint4		patch_save_count;

void dse_rmsb(void)
{
	block_id	blk;
#ifndef BLK_NUM_64BIT
	block_id_64	blk2;
#endif
	char		util_buff[MAX_UTIL_LEN];
	int		util_len;
	uint4		found_index, version;
	unsigned int	i;

	if (cli_get_int("VERSION", (int4 *)&version))
	{
		if (0 == version)
		{
			util_out_print("Error:  no such version.", TRUE);
			return;
		}
	} else
		version = 0;
	/* don't use dse_getblk - working out of the save set, not the db */
#ifdef BLK_NUM_64BIT
	if (!cli_get_hex64("BLOCK", (gtm_uint8 *)&blk))
		blk = patch_curr_blk;
#else
	if (!cli_get_hex64("BLOCK", (gtm_uint8 *)&blk2))
		blk = patch_curr_blk;
	else
	{
		assert(blk2 == (block_id_32)blk2);
		blk = (block_id_32)blk2;
	}
#endif
	found_index = 0;
	for (i = 0; i < patch_save_count; i++)
	{
		if ((patch_save_set[i].blk == blk) && (patch_save_set[i].region == gv_cur_region))
		{
			if (version == patch_save_set[i].ver)
			{
				assert(version);
				found_index = i + 1;
				break;
			}
			if (!version)
			{
				if (found_index)
				{
					util_out_print("Error:  save version number must be specified.", TRUE);
					return;
				}
				found_index = i + 1;
			}
		}
	}
	if (0 == found_index)
	{
		if (version)
			util_out_print("Error: Version !UL of block 0x!16@XQ not found in set of saved blocks", TRUE, version, &blk);
		else
			util_out_print("Error: Block 0x!16@XQ not found in set of saved blocks", TRUE, &blk);
		return;
	}
	if (!version)
	{
		i = found_index - 1;
		version = patch_save_set[i].ver;
	}
	util_len = SIZEOF("!/Removing version !UL of block ");
	memcpy(util_buff, "!/Removing version !UL of block ", util_len);
	util_len += i2hexl_nofill(blk, (uchar_ptr_t)&util_buff[util_len-1], MAX_HEX_INT8);
	util_buff[util_len-1] = 0;
	assert(ARRAYSIZE(util_buff) >= util_len);
	util_out_print(util_buff, TRUE, version);
	free(patch_save_set[i].bp);
	memmove(&patch_save_set[i], &patch_save_set[i + 1], (--patch_save_count - i) * SIZEOF(save_strct));
	return;
}
