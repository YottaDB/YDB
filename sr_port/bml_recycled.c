/****************************************************************
 *								*
 * Copyright (c) 2007-2023 Fidelity National Information	*
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
#include "bit_set.h"

GBLREF	boolean_t	dse_running;
GBLREF  uint4		mu_upgrade_in_prog;

uint4 bml_recycled(block_id setfree, sm_uc_ptr_t map)
{
	uint4	ret, ret1;

	/* This function is specifically for local maps so the block index
	 * setfree should not be larger then BLKS_PER_LMAP
	 */
	assert(BLKS_PER_LMAP > setfree);

	setfree *= BML_BITS_PER_BLK;
	ret = bit_set(setfree, map);
	ret1 = bit_set(setfree + 1, map);
	assert((!ret && !ret1) || dse_running || mu_upgrade_in_prog);
	return ret;
}
