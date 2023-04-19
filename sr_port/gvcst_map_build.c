/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdsbml.h"

#include "send_msg.h"		/* prototypes */
#include "gvcst_map_build.h"
#include "min_max.h"
#include "wbox_test_init.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	uint4		mu_upgrade_in_prog;

void gvcst_map_build(block_id *array, sm_uc_ptr_t base_addr, cw_set_element *cs, trans_num ctn)
{
	boolean_t	status;
	uint4		(*bml_func)();
	uint4		ret;
	block_id	bitnum;
#ifdef DEBUG
	int4		actual_cnt = 0;
	block_id	prev_bitnum = -1;

	if (!ydb_white_box_test_case_enabled || (WBTEST_ANTIFREEZE_DBBMLCORRUPT != ydb_white_box_test_case_number))
	{
		VALIDATE_BM_BLK(cs->blk, (blk_hdr_ptr_t)base_addr, cs_addrs, gv_cur_region, status);
		assert(status); /* assert it is a valid bitmap block */
	}
#endif
	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	base_addr += SIZEOF(blk_hdr);
	assert(cs_addrs->now_crit); /* Don't want to be messing with highest_lbm_with_busy_blk outside crit */
	DETERMINE_BML_FUNC(bml_func, cs, cs_addrs);
	while (bitnum = *array)		/* caution : intended assignment */
	{
		assert(bitnum == (int4)bitnum);		/* check that casting bitnum is valid */
		assert((int4)bitnum < cs_addrs->hdr->bplmap);	/* check that bitnum is positive and within 0 to bplmap */
		assert(bitnum > prev_bitnum);	/* assert that blocks are sorted in the update array */
		ret = (* bml_func)(bitnum, base_addr);
		DEBUG_ONLY(
			if (cs->reference_cnt > 0)
				actual_cnt++;	/* block is being marked busy */
			else if (!ret)
				actual_cnt--;	/* block is transitioning from BUSY to either RECYCLED or FREE */
			/* all other state changes do not involve updates to the free_blocks count */
		)
		array++;
		DEBUG_ONLY(prev_bitnum = bitnum;)
	}
	assert(actual_cnt == cs->reference_cnt || mu_upgrade_in_prog);
}
