/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

int4 dse_lm_blk_free(int4 blk, sm_uc_ptr_t base_addr)
{
	sm_uc_ptr_t 	ptr;
	unsigned char	valid;
	int4		bits;

	blk /= BML_BITS_PER_BLK;
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
