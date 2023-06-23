/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "patcode.h"

/* The below array is initialized based on the character classes ordering in PATM_CODELIST.
 * For example PATM_UTF8_ALPHABET = (1 << 5). So patmask_seq[5+1] = 22 because 0 (the external name assigned to
 *	PATM_UTF8_ALPHABET) is the 22nd character (counting from 0) in PATM_CODELIST.
 * For example PATM_L = (1 << 2). So patmask_seq[2+1] = 2 because L is the 2nd character (counting from 0) in PATM_CODELIST.
 */
static	int	patmask_seq[] =
{
     -1, 0,  1,  2,  3,  4, 22, 23, -1,
	 5,  6,  7,  8,  9, 10, 11, 12,
	13, 14, 15, 16, 17, 18, 19, 20,
	21, -1, -1, -1, -1, -1, -1, -1
};

/* Returns -1 if the lowest bit in pattern_mask that is set to 1 happens to be an invalid pattern mask bit.
 * Otherwise returns a sequence number different for different lowest bit numbers that are valid pattern mask bits.
 */
int patmaskseq(uint4 pattern_mask)
{
	int	index;
	uint4	value;

	value = pattern_mask & PATM_LONGFLAGS;
	value -= value & (value - 1);	/* this transformation removes all 1-bits in value except for the lowest one */
	/* value, if non-zero, now has only one bit, the lowest bit set. now find out the number of the lowest bit */
	for (index = 0; value; index++)
		value >>= 1;
	return patmask_seq[index];
}
