/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "five_bit.h"

/* five_bit - convert 3-character string into 5-bit character representation */

unsigned short five_bit(unsigned char *src) /* src is pointer to 3-character string to be converted to 5-bit format */
{
	int		index;
	unsigned short	result;

	/* Or low-order 5 bits of each character together into high-order 15 bits of result.  */
	for (index = 0, result = 0;  index < 3;  index++, src++)
		result = (result << 5) | (*src & 0x1f);
	result <<= 1;

	return result;
}
