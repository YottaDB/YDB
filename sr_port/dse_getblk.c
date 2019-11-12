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
#include "dse.h"
#include "gtmmsg.h"

GBLREF block_id		patch_curr_blk;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

error_def(ERR_BLKINVALID);
error_def(ERR_CANTBITMAP);

block_id dse_getblk(char *element, boolean_t nobml, boolean_t carry_curr)
{
	block_id	blk;
#ifndef BLK_NUM_64BIT
	block_id_64	blk2;
#endif

#ifdef BLK_NUM_64BIT
	if (cli_get_hex64(element, (gtm_uint8 *)(&blk)))
		CLEAR_DSE_COMPRESS_KEY;
	else
		blk = patch_curr_blk;
#else
	if (cli_get_hex64(element, (gtm_uint8 *)(&blk2)))
	{
		assert(blk2 == (block_id_32)blk2); /* Verify that blk2 won't overflow an int4 */
		blk = (block_id_32)blk2;
		CLEAR_DSE_COMPRESS_KEY;
	} else
		blk = patch_curr_blk;
#endif
	if ((blk < 0) || (blk >= cs_addrs->ti->total_blks))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_BLKINVALID, 4, blk, DB_LEN_STR(gv_cur_region),
			cs_addrs->ti->total_blks);
		return BADDSEBLK;
	}
	if (nobml && IS_BITMAP_BLK(blk))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CANTBITMAP);
		return BADDSEBLK;
	}
	if (carry_curr)
		patch_curr_blk = blk;
	return blk;
}
