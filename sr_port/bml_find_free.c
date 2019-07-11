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
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsfhead.h"

#define	RETURN_IF_FREE(valid, ptr, base_addr)							\
{												\
	int4	bits;										\
												\
	if (valid)										\
	{											\
		if (valid > THREE_BLKS_BITMASK)							\
			bits = 3;								\
		else if (valid > TWO_BLKS_BITMASK)						\
			bits = 2;								\
		else if (valid > ONE_BLK_BITMASK)						\
			bits = 1;								\
		else										\
			bits = 0;								\
		return (int4)((ptr - base_addr) * (BITS_PER_UCHAR / BML_BITS_PER_BLK) + bits);	\
	}											\
}

/* Returns the location of the first set bit in the field. The search starts at the hint and wraps if necessary.
 * If a non-null update array is passed, that is also taken into account while figuring out if any free block is available.
 */
int4 bml_find_free(block_id hint, uchar_ptr_t base_addr, block_id total_bits)
{
	uchar_ptr_t	ptr, top;
	unsigned char 	valid;
	block_id	bits;

	/* This function is specifically for local maps so the block indexes
	 * hint and total_bits should not be larger then BLKS_PER_LMAP
	 */
	assert(BLKS_PER_LMAP >= total_bits);

	hint *= BML_BITS_PER_BLK;
	hint = hint / BITS_PER_UCHAR;
	total_bits *= BML_BITS_PER_BLK;

	if (hint > DIVIDE_ROUND_UP(total_bits, BITS_PER_UCHAR))
		hint = 0;
	for (ptr = base_addr + hint, top = base_addr + DIVIDE_ROUND_UP(total_bits, BITS_PER_UCHAR) - 1; ptr < top; ptr++)
	{
		valid = *ptr;
		RETURN_IF_FREE(valid, ptr, base_addr);
	}
	if (*ptr)	/* Special processing for last byte as may be only partially valid */
	{
		bits = total_bits % BITS_PER_UCHAR;
		if (bits == 6)
			valid = *ptr & THREE_BLKS_BITMASK;
		else if (bits == 4)
			valid = *ptr & TWO_BLKS_BITMASK;
		else if (bits == 2)
			valid = *ptr & ONE_BLK_BITMASK;
		else
			valid = *ptr;
		RETURN_IF_FREE(valid, ptr, base_addr);
	}
	if (hint)
	{
		for (ptr = base_addr, top = base_addr + hint; ptr < top; ptr++)
		{
			valid = *ptr;
			RETURN_IF_FREE(valid, ptr, base_addr);
		}
	}
	return NO_FREE_SPACE;
}
