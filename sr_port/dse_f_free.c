/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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
#include "gdscc.h"
#include "t_qread.h"
#include "gdsfilext.h"
#include "dse.h"
#include "getfree_inline.h"
#include "filestruct.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;

error_def(ERR_DSEBLKRDFAIL);

void dse_f_free(void)
{
	block_id	blk, hint;
	char		util_buff[MAX_UTIL_LEN];
	int4		bplmap;
	int4		lmap_bit, nocrit_present, util_len;

	DSE_DB_IS_TOO_OLD(cs_addrs, cs_data, gv_cur_region);
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	if (0 == (bplmap = cs_addrs->hdr->bplmap))					/* WARNING: assignment */
	{	util_out_print("Cannot perform free block search:  bplmap field of file header is zero.", TRUE);
		return;
	}
	if (BADDSEBLK == (hint = dse_getblk("HINT", DSEBMLOK, DSEBLKNOCUR)))		/* WARNING: assignment */
		return;
	blk = SIMPLE_FIND_FREE_BLK(hint, nocrit_present, FALSE);
	if (NO_FREE_SPACE == blk)
		util_out_print("Error: database full.", TRUE);
	else if (MAP_RD_FAIL == blk)
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) nocrit_present ? ERR_DSEBLKRDFAIL : MAKE_MSG_INFO(ERR_DSEBLKRDFAIL));
	else if (0 == (blk - ((blk / bplmap) * bplmap)))
	{
		memcpy(util_buff, "Error: bit map in block ", SIZEOF("Error: bit map in block "));
		util_len = SIZEOF("Error: bit map in block ") - 1;
		util_len += i2hexl_nofill(blk, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
		memcpy(&util_buff[util_len], " incorrectly marked free in master map.",
		       SIZEOF(" incorrectly marked free in master map.")) - 1;
		util_len += 39;
	} else
	{
		memcpy(util_buff, "!/Next free block is ", SIZEOF("!/Next free block is "));
		util_len = SIZEOF("!/Next free block is ") - 1;
		util_len += i2hexl_nofill(blk, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
		memcpy(&util_buff[util_len], ".!/", SIZEOF(".!/")) - 1;
		util_len += SIZEOF(".!/");
	}
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
	return;
}
