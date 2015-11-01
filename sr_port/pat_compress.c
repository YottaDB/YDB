/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "copy.h"
#include "patcode.h"
#include "min_max.h"

/* If a pattern contains consecutive atoms that could be combined, this procedure will attempt to do so.
 * For instance both .E.N and .N.E could be simplified to .E
 */
int pat_compress(uint4	pattern_mask,
	void		*strlit_buff,
	int4		strlen,
	boolean_t	infinite,
	boolean_t	last_infinite,
	uint4		*lastpatptr)
{
	uint4	y_max;
	uint4	lastpat_mask;

 	assert(pattern_mask);
	if ((infinite == last_infinite) && !(pattern_mask & PATM_ALT))
	{
		lastpat_mask = *lastpatptr & PATM_LONGFLAGS;
		if (lastpat_mask == pattern_mask)
		{
			if (pattern_mask & PATM_STRLIT)
			{
				if (!memcmp(lastpatptr + 1, strlit_buff, strlen + sizeof(int4)))
					return TRUE;
			} else
				return TRUE;
		} else if (infinite && lastpat_mask && !(pattern_mask & PATM_STRLIT))
		{	/* lastpat_mask can be 0 if the previous pattern mask was PATM_ALT which is outside of PATM_LONGFLAGS range.
			 * Do not compress in that case. Hence the && lastpat_mask in the else if above.
			 */
			y_max = MAX(lastpat_mask, pattern_mask);
			/* Check if one pattern subsumes the other. The check is a simple bit-wise OR condition.
			 * This is because lastpat_mask and pattern_mask are non-zero here.
			 */
			if ((lastpat_mask | pattern_mask) == y_max)
			{
				*lastpatptr = y_max;
				return TRUE;
			}
		}
	}
	return FALSE;
}
