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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

/* Include prototypes */
#include "t_qread.h"


/*
get_lmap.c:
	Reads local bit map and returns buffer address,
	two bit local bit-map value corresponding to the block, cycle and cr
Input Parameter:
	blk: block id of the block whose bit map this routine is to fetch
Output Parameter:
	bits: two bit local bit map
	cycle: Cycle value found in t_qread
	cr: Cache Record value found in t_qread
Returns:
	buffer address of local bitmap block
	Null: if t_qread fails
*/
sm_uc_ptr_t get_lmap (block_id blk, unsigned char *bits, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr)
{
	sm_uc_ptr_t 	ptr, bp;
	block_id	index, offset;
	error_def(ERR_DSEBLKRDFAIL);

	index = ROUND_DOWN2(blk, BLKS_PER_LMAP);
	offset = blk - index;
	bp = t_qread (index, cycle, cr);
	if (bp)
	{
		ptr =  bp + SIZEOF(blk_hdr) + (offset * BML_BITS_PER_BLK) / 8;
		*bits = *ptr;
		switch (blk % (8 / BML_BITS_PER_BLK))
		{	case 0:	break;
			case 1:
				*bits = *bits >> BML_BITS_PER_BLK;
				break;
			case 2:
				*bits = *bits >> 2 * BML_BITS_PER_BLK;
				break;
			case 3:
				*bits = *bits >> 3 * BML_BITS_PER_BLK;
				break;
		}
		*bits = *bits & 3;
	}
	return bp;
}
