/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "patcode.h"

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
