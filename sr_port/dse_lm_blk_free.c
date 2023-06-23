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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "cli.h"
#include "dse.h"

/**
 * Returns a bit mask of the status of a block in a local bitmap
 *
 * Takes an index for a block in a local bit map (not the block number) and a pointer to a local bit map.
 * Returns the 2-bit status of the block corresponding to block index in the local bit map.
 *
 * @param[in] blk The index of a block in it's local bit map (block_id % BLKS_PER_LMAP)
 * @param[in] base_addr Pointer to the start of the local bit map
 * @return A bit mask of the status of the block at index blk in local bit map base_addr
 */
int4 dse_lm_blk_free(int4 blk, sm_uc_ptr_t base_addr)
{
	sm_uc_ptr_t 	ptr;
	unsigned char	valid;
	int4		bits;

	/* blk is an index into a local bit map and should never be larger the BLKS_PER_LMAP */
	assert(blk <= BLKS_PER_LMAP);

	ptr = base_addr + (blk * BML_BITS_PER_BLK) / 8;
	valid = *ptr;
	switch (blk % (8 / BML_BITS_PER_BLK))
	{	case 0:	break;
		case 1:
			valid = valid >> BML_BITS_PER_BLK;
			break;
		case 2:
			valid = valid >> 2 * BML_BITS_PER_BLK;
			break;
		case 3:
			valid = valid >> 3 * BML_BITS_PER_BLK;
			break;
	}
	bits = valid & 3;
	return bits;
}
