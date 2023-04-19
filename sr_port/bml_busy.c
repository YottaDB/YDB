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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsfhead.h"

/* Include prototypes */
#include "bit_clear.h"
#include "wbox_test_init.h"

GBLREF	boolean_t	dse_running;
GBLREF	uint4		mu_upgrade_in_prog;

uint4 bml_busy(block_id setbusy, sm_uc_ptr_t map)
{
	uint4	ret, ret1;

	/* This function is specifically for local maps so the block index
	 * setbusy should not be larger then BLKS_PER_LMAP
	 */
	assert(BLKS_PER_LMAP > setbusy);
	setbusy *= BML_BITS_PER_BLK;
	ret = bit_clear(setbusy, map);
	ret1 = bit_clear(setbusy + 1, map);
	/* In case of a valid snapshot, assert that only a RECYCLED or FREE block gets marked as BUSY
	 * (dse and MUPIP UPGRADE are exceptions).
	 */
	assert((ret && ret1) || (ret && !ret1) || dse_running || (mu_upgrade_in_prog && (DIR_ROOT + 1 == setbusy))
		|| (WBTEST_INVALID_SNAPSHOT_EXPECTED == gtm_white_box_test_case_number));
	return ret;
}
