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

#include <builtins.h>

/* __ADAWI on Alpha returns a simulated VAX PSL, NOT a value representing a condition code (-1, 0, or +1).
   adawi is defined here as a static routine to convert the PSL to an appropriate value.   */
int adawi(short x, short *y)
{
	int vax_psl = __ADAWI(x, y);
	if (vax_psl & 4 /* Z bit */) return 0;
	if (vax_psl & 8 /* N bit */) return -1;
	return 1;
}
