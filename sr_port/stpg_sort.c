/****************************************************************
 *								*
 * Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "stpg_sort.h"

#define	NUM_ELEMS_FOR_MEDIAN		8
#define	INSERTION_SORT_CUTOFF		15
#define	MAX_QSORT_RECURSION_DEPTH	32

/* "stpg_quicksort" sorts an array of pointers to mstr's using the value of each mstr's addr field as the key.
 * The algorithm used is a modified Quicksort (modification is in choice of Pivot).
 */
void stpg_sort (mstr **base, mstr **top)
{
	mstr	**stack[2 * MAX_QSORT_RECURSION_DEPTH];	/* 2 entries are added to stack[] per recursion depth */
	mstr	***sp;
	mstr	**median[NUM_ELEMS_FOR_MEDIAN + 1];
	mstr	*v, *t;
	mstr	**l, **r, **curl;
	mstr	**ix, **jx, **kx;
	char	*tval;
	int	cnt, cnti, cntj, max_depth_allowed, spacing;
#	ifdef DEBUG
	mstr	***sp_top;
#	endif

	l = base;
	r = top;
	sp = stack;
	DEBUG_ONLY(sp_top = stack + ARRAYSIZE(stack));
	for (;;)
	{
		if (INSERTION_SORT_CUTOFF >= (r - l))
		{	/* The # of elements is small enough so use Insertion sort (optimal for sorting small arrays) */
			for (ix  = l + 1;  ix <= r;  ix++)
			{
				for (jx = ix, t= *ix, tval = t->addr;  jx > l  &&  (*(jx - 1))->addr > tval;  jx--)
					*jx = *(jx - 1);
				*jx = t;
			}
			/* Now that this subset of an array is sorted, check if there are any more array partitions that
			 * need sorting (stored in the "sp" stack).
			 */
			if (sp <= stack)
			{
				assert(sp == stack);
				break;
			} else
			{
				l = *--sp;
				r = *--sp;
			}
		} else
		{	/* Use Quicksort. Compute Pivot as median of 9 equally-spaced indices in the array [l,r] */
			spacing = (int)(r - l) / NUM_ELEMS_FOR_MEDIAN;
			assert(2 <= spacing);
			for (curl = l, cnt = 0; cnt <= NUM_ELEMS_FOR_MEDIAN; curl += spacing)
				median[cnt++] = curl;
			for (cnti = 1;  cnti <= NUM_ELEMS_FOR_MEDIAN; cnti++)
			{
				kx = median[cnti];
				tval = (*kx)->addr;
				for (cntj = cnti - 1; (0 <= cntj) && ((*median[cntj])->addr > tval); cntj--)
					median[cntj + 1] = median[cntj];
				median[cntj + 1] = kx;
			}
			/* Now that the median[] array is sorted, the median can be found from the array midpoint */
			assert((NUM_ELEMS_FOR_MEDIAN + 1) == ARRAYSIZE(median));
			kx = median[NUM_ELEMS_FOR_MEDIAN / 2];	/* This is the median and hence the pivot for quicksort */
			ix = l;
			jx = r;
			v = *kx;
			*kx = *jx;
			*jx = v;
			tval = v->addr;
			ix--;
			do
			{
				do
				{
					ix++;
				} while ((*ix)->addr < tval);
				do
				{
					jx--;
				} while ((*jx)->addr > tval);
				t = *ix;
				*ix = *jx;
				*jx = t;
			} while (jx > ix);
			*jx = *ix;
			*ix = *r;
			*r = t;
			/* Done with partitioning the array [l,r] into 3 parts : [l,ix-1] [ix] [ix+1,r]
			 * where the pivot element is in [ix]. Now move on to sort the smaller of the two
			 * sub-arrays [l,ix-1] or [ix+1,r] in the next iteration of this for loop and
			 * store the other sub-array in the recursion stack "sp" for a later iteration.
			 */
			assert((sp + 1) < sp_top); /* Ensure no overflow in our recursion stack after below *sp++ operations */
			if (ix - l > r - ix)
			{	/* [ix+1,r] is the smaller sub-array so finish this first and store [l,ix-1] for later */
				*sp++ = ix - 1;
				*sp++ = l;
				l = ix + 1;
			} else
			{	/* [l,ix-1] is the smaller sub-array so finish this first and store [ix+1,r] for later */
				*sp++ = r;
				*sp++ = ix + 1;
				r = ix - 1;
			}
		}
	}
	return;
}
