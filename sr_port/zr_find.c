/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "zbreak.h"

/* Finds a breakpoint record for input "addr" in array of breakpoint records "zrecs".
 * If "return_closest_match" is FALSE, it returns NULL if matching breakpoint record is not found.
 * If "return_closest_match" is TRUE , it returns matching breakpoint record if one is found and if
 *	not, it returns the breakpoint record BEFORE where the matching breakpoint record would have
 *	been found had it been there in the array.
 */
zbrk_struct *zr_find(z_records *zrecs, zb_code *zb_addr, boolean_t return_closest_match)
{
	zbrk_struct	*bot, *top, *mid;
	zb_code		*zb_mpc;

	/* NOTE: records are stored by decreasing addresses */
	for (bot = zrecs->beg, top = zrecs->free; bot < top; )
	{
		/* At every iteration, [bot,top) is the set of records that can have the desired "zb_addr".
		 * Note: the interval is closed "[" on the "bot" side and open ")" on the "top" side which
		 * means bot is included in the set of potentially matching records but top is not.
		 */
		mid = bot + (top - bot) / 2;
		assert(mid >= bot);
		zb_mpc = mid->mpc;
		if (zb_mpc == zb_addr)
			return mid;
		else if (zb_mpc > zb_addr)
			bot = mid + 1;
		else
		{
			assert(mid < top);
			top = mid;
		}
	}
	assert(top == bot);	/* only then would we have exited the above for loop */
	return (return_closest_match ? (top - 1) : NULL);
}
