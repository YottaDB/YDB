/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF block_id		patch_curr_blk;
GBLREF save_strct	patch_save_set[PATCH_SAVE_SIZE];
GBLREF unsigned short	patch_save_count;

void dse_rmsb(void)
{
	block_id	blk;
	unsigned int	i;
	uint4		version;
	bool		found;

	if (cli_present("VERSION") != CLI_PRESENT)
	{
		util_out_print("Error:  save version number must be specified.", TRUE);
		return;
	}
	if (!cli_get_int("VERSION", (int4 *)&version))
		return;
	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
			return;
		if (blk < 0 || blk >= cs_addrs->ti->total_blks || !(blk % cs_addrs->hdr->bplmap))
		{
			util_out_print("Error: invalid block number.", TRUE);
			return;
		}
		patch_curr_blk = blk;
	}
	found = FALSE;
	for (i = 0;  i < patch_save_count;  i++)
		if (patch_save_set[i].blk == patch_curr_blk && patch_save_set[i].region == gv_cur_region
			&&(found = version == patch_save_set[i].ver))
			break;
	if (!found)
	{
		util_out_print("Error:  no such version.", TRUE);
		return;
	}
	patch_save_count--;
	free(patch_save_set[i].bp);
	memcpy(&patch_save_set[i], &patch_save_set[i + 1],
				(patch_save_count - i) * SIZEOF(save_strct));
	return;
}
