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
#include "getfree_blk_inline.h"

/* Returns the location of the first set bit in the field. The search starts at the hint and wraps if necessary.
 * If a non-null update array is passed, that is also taken into account while figuring out if any free block is available.
 */
int4 bml_find_free(block_id hint, uchar_ptr_t base_addr, block_id total_bits)
{
	u_char		back, bits, front, valid;
	uchar_ptr_t	ptr, top;

	/* This function is specifically for local maps so the block indexes
	 * hint and total_bits should not be larger then BLKS_PER_LMAP
	 */
	assert(BLKS_PER_LMAP >= total_bits);
	ptr = (hint / BML_BLKS_PER_UCHAR) + base_addr;
	top = base_addr + (total_bits / BML_BLKS_PER_UCHAR); 			/* no round up enables odd treatment of last byte */
	switch (total_bits % BML_BLKS_PER_UCHAR)
	{	/* set up back mask for last byte in case of partial bit map; masks "nibble" into the last byte */
		case 0:
			back = 0;							/* "normal" case */
			break;
		case 1:
			back = ONE_BLK_BITMASK;
			break;
		case 2:
			back = TWO_BLKS_BITMASK;
			break;
		case 3:
			back = THREE_BLKS_BITMASK;
			break;
		default:
			assert(FALSE);
			back = 0;
	}
	switch (bits = (((hint) > total_bits) ? 0 : (hint) % BML_BLKS_PER_UCHAR))	/* out of bounds hint "wraps" */
	{	/* set up front mask for first byte in case hint is past lower blocks; masks are the complement of back masks */
		case 0:
			front = 0;							/* "normal"/"default case */
			break;
		case 1:
			front = ((~ONE_BLK_BITMASK) & BYTEMASK);
			break;
		case 2:
			front = ((~TWO_BLKS_BITMASK) & BYTEMASK);
			break;
		case 3:
			front = ((~THREE_BLKS_BITMASK) & BYTEMASK);
			break;
		default:
			assert(FALSE);
			front = 0;
	}
	if (front)
	{	/* starting past the beginning of the byte due to hint */
		valid = *ptr & front;
		if (back && (ptr >= top))
			valid &= back;							/* 1st byte is also the last */
		RETURN_IF_FREE(valid, ptr, base_addr);					/* move on to the next byte */
		ptr++;									/* ++ ignored on macro/inline argument  */
	}
	for (; ptr < top; ptr++)
	{										/* main scan loop */
		valid = *ptr;
		RETURN_IF_FREE(valid, ptr, base_addr);
	}
	if (back && !front)
	{	/* partial last byte */
		assert(ptr >= top);
		valid = *ptr & back;
		RETURN_IF_FREE(valid, ptr, base_addr);
	}
	for (ptr = base_addr, top = base_addr + DIVIDE_ROUND_UP(hint, BML_BLKS_PER_UCHAR); ptr < top; ptr++)
	{	/* if no free found, wrap working from base_addr back to hint byte */
		valid = *ptr;
		RETURN_IF_FREE(valid, ptr, base_addr);
	}
	return NO_FREE_SPACE;
}
