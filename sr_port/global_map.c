/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "global_map.h"
#include "min_max.h"

/* "map[]" is assumed to be an array of mstrs whose last mstr has a NULL value for its "addr" field.
 * "map[]" contains even number of mstrs, each consecutive pair of mstrs (starting from map[0]) point to a range of names.
 * Also "map[]" is an array of strings arranged in non-decreasing alphabetical order.
 * Thus "map[]" is an array representing a set of ranges starting from the smallest to the largest.
 * "beg" and "end" are the end-points of the range (both inclusive) to be inserted into this sorted range set.
 * Note that a point is represented as a range whose begin and end are the same.
 */

void global_map(mstr map[], mstr *beg, mstr *end)
{
	mstr	*left, *right, rangestart, rangeend, tmpmstr;
	int	rslt;

	DEBUG_ONLY(
		MSTRP_CMP(beg, end, rslt);
		assert(0 >= rslt);
	)
	for (left = map; left->addr; left++)
	{
		MSTRP_CMP(left, beg, rslt);
		if (0 <= rslt)
			break;
	}
	/* left now points to the first mstr in the array that is >= beg */
	for (right = left; right->addr; right++)
	{
		MSTRP_CMP(right, end, rslt);
		if (0 < rslt)
			break;
	}
	/* right now points to the first mstr in the array that is > end */
	if (left == right)
	{	/* the usual case where {beg, end} has no or complete intersections with any existing range in map[].
		 * in the case of complete intersection return.
		 * in case of no intersection, insert {beg, end} maintaining sorted order by shifting all higher existing ranges
		 * 	two positions to the right */
		if ((right - map) & 1)
			return;
		rangestart = *beg;
		rangeend = *end;
		while (left->addr)
		{	/* {rangestart, rangeend} is the current range to be inserted */
			tmpmstr = *left;
			*left++ = rangestart;
			rangestart = tmpmstr;
			tmpmstr = *left;
			*left++ = rangeend;
			rangeend = tmpmstr;
		}
		*left++ = rangestart;
		*left++ = rangeend;
		left->addr = 0;
		return;
	}
	/* case where {beg, end} has partial intersections with existing ranges in map[].
	 * replace intersecting ranges with one union range e.g. replace {1, 10} {5, 15} {12, 20} with {1, 20}
	 */
	if (0 == ((left - map) & 1))
		*left++ = *beg;
	if (0 == ((right - map) & 1))
		*(--right) = *end;
	if (left == right) /* possible if {beg, end} is exactly equal to an existing range in map[] */
		return;
	do
	{
		*left++ = *right;
	} while ((right++)->addr);
	/* note that replacing atleast 2 existing ranges with {begin, end} into one union range will cause a reduction
	 * in the number of ranges in map[] effectively causing higher-valued ranges on the right to shift left.
	 * In that case, we have to be also left-shift the null-valued mstr.addr, hence the ++ is done in the check
	 * for (right++)->addr in the "while" above instead of having "*left++ = *right++" in the loop.
	 */
	return;
}
