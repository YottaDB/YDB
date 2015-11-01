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
#include "gtm_ffs.h"

#define BITS_PER_UCHAR	8

int gtm_ffs (uint4 offset, uchar_ptr_t addr, uint4 size)
{
	uchar_ptr_t	c;
	int		i, j, top;

	c = addr + (offset / BITS_PER_UCHAR);
	if (i = (offset & (BITS_PER_UCHAR - 1)))
	{	/* partial byte starting at offset */
		for (j = 0;  (i < BITS_PER_UCHAR) && (j < size);  j++, i++)
		{
			if (*c & (1 << i))
				return (offset + j);
		}
		c++;
	}
	assert(c == (addr + (offset + BITS_PER_UCHAR - 1) / BITS_PER_UCHAR));
	for (i = ROUND_UP2(offset, BITS_PER_UCHAR), top = ROUND_DOWN2(size + offset, BITS_PER_UCHAR);  i < top;
		c++, i += BITS_PER_UCHAR)
	{	/* full bytes offset to end */
		if (*c)
		{
			for (j = 0;  j < BITS_PER_UCHAR;  j++)
			{
				if (*c & (1 << j))
					return (i + j);
			}
		}
	}
	for (j = 0, top = size + offset;  i < top;  j++, i++)
	{	/* partial byte at end */
		assert(j < BITS_PER_UCHAR);
		if (*c & (1 << j))
			return i;
	}
	return -1;
}
