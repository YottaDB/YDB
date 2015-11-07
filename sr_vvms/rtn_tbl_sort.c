/****************************************************************
 *								*
 *	Copyright 2004, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* NOTE: The rtn_tbl_qsort below uses exactly the same quick sort algorithm used in stpg_sort() and gvcst_kill_sort() */

#include "mdef.h"
#include <rtnhdr.h>
#include "obj_file.h"		/* for RNAMB_PREF_LEN */
#include "objlangdefs.h"	/* for EGPS$S_NAME */
#include "min_max.h"		/* MIDENT_CMP needs MIN */

#define S_CUTOFF		15

void rtn_tbl_qsort(rtn_tabent *start, rtn_tabent *end);

void rtn_tbl_sort(rtn_tabent *rtab_base, rtn_tabent *rtab_end)
{
	rtn_tabent	*start, *end;

	for (start = rtab_base; start < rtab_end; start = end)
	{ /* Since the table is already sorted by the VMS linker based on the first 26 characters of rt_name,
	     we only need to sort those routines with longer than 26 char names. However, if the routine name
	     starts with 26 all z's, it may not have been sorted. So, to avoid special cases, each iteration
	     of this loop needs to find a contiguous segment of routines of more than 25 chars and invoke
	     the quicksort on that segment. */
		for (; start < rtab_end && start->rt_name.len < RNAME_SORTED_LEN; start++)
			;
		for (end = start; end < rtab_end && end->rt_name.len >= RNAME_SORTED_LEN; end++)
			;
		if (start < end)
		{
			if (end < rtab_end)
				rtn_tbl_qsort(start, end - 1);
			else
				rtn_tbl_qsort(start, end);
		}
	}
}

/* Sorts a segment of contiguous routine table entries using the value of each rt_name.addr field as the key.
 * The algorithm is a modified QuickSort algorithm which assumes that top+1 has a greater key value than
 * any element in the segment. */
void rtn_tbl_qsort(rtn_tabent *base, rtn_tabent *top)
{
	/* Since a pair of pointers are pushed at each level, the stack space of 64 is sufficient
	 * upto a table size of 2**32 elements */
	rtn_tabent	*stack[64], **sp;
	rtn_tabent	v, t;
	rtn_tabent	*l, *r;
	rtn_tabent	*ix, *jx, *kx;
	mident		*tval;
	int		cmp;

	sp = &stack[0];
	l = base;
	r = top;
	for (;;)
	{
		if (r - l < S_CUTOFF)
		{
			for (ix  = l + 1;  ix <= r;  ix++)
			{
				for (jx = ix, t = *ix, tval = &t.rt_name; 1 < jx; jx--)
				{
					MIDENT_CMP(&(jx - 1)->rt_name, tval, cmp);
					if (0 >= cmp)
						break;
					*jx = *(jx - 1);
				}
				if (ix != jx)
					*jx = t;
			}
			if (sp <= stack)
				break;
			else
			{	/* Pop the anchors of a subtable that were pushed earlier and begin a new sort */
				l = *--sp;
				r = *--sp;
			}
		} else
		{
			ix = l;
			jx = r;
			kx = l + ((int)(r - l) / 2); /* pivotal key */
			/* Find the best possible pivotal key among ix, jx and kx (sorta median of those) */
			MIDENT_CMP(&ix->rt_name, &jx->rt_name, cmp);
			if (0 < cmp)
			{
				MIDENT_CMP(&jx->rt_name, &kx->rt_name, cmp);
				if (0 < cmp)
					kx = jx;
				else
				{
					MIDENT_CMP(&ix->rt_name, &kx->rt_name, cmp);
					if (0 >= cmp)
						kx = ix;
					/* else "kx" is already the right choice */
				}
			} else
			{
				MIDENT_CMP(&jx->rt_name, &kx->rt_name, cmp);
				if (0 > cmp)
					kx = jx;
				else
				{
					MIDENT_CMP(&ix->rt_name, &kx->rt_name, cmp);
					if (0 < cmp)
						kx = ix;
					/* else "kx" is already the right choice */
				}
			}
			/* Partition the table into two subtables based on the pivotal */
			v = *kx;
			*kx = *jx;
			*jx = v;
			tval = &v.rt_name;
			ix--;
			do
			{
				do
				{
					ix++;
					MIDENT_CMP(&ix->rt_name, tval, cmp);
				} while (cmp < 0);
				do
				{
					jx--;
					MIDENT_CMP(&jx->rt_name, tval, cmp);
				} while (cmp > 0);
				t = *ix;
				*ix = *jx;
				*jx = t;
			} while (jx > ix);
			*jx = *ix;
			*ix = *r;
			*r = t;

			/* Ensure there are at least two more slots available in the stack for the pushes below */
			assert((sp - &stack[0]) + 2 < (SIZEOF(stack)/SIZEOF(stack[0])));
			/* Push the anchors of the large subtable into the stack and begin a new sort on the smaller subtable */
			if (ix - l > r - ix)
			{
				*sp++ = ix - 1;
				*sp++ = l;
				l = ix + 1;
			}
			else
			{
				*sp++ = r;
				*sp++ = ix + 1;
				r = ix - 1;
			}
		}
	}
	return;
}
