/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gcol_list.h"
#include "stpg_sort.h"

#define S_CUTOFF 15

/*	stpg_sort sorts an array of pointers to mstr's using the value
	of each mstr's addr field as the key.  The algorithm is a modified
	QuickSort algorithm.
*/

void stpg_sort (mstr **base, mstr **top)
{
	mstr	**stack[100], ***sp;
	mstr	*v, *t;
	mstr	**l, **r;
	mstr	**ix, **jx, **kx;
	char	*tval;

	sp = stack;
	l = base;
	r = top;
	for (;;)
	{
		if (r - l < S_CUTOFF)
		{
			for (ix  = l + 1;  ix <= r;  ix++)
			{
				for (jx = ix, t= *ix, tval = t->addr;  jx > l  &&  (*(jx - 1))->addr > tval;  jx--)
				{
					*jx = *(jx - 1);
				}
				*jx = t;
			}
			if (sp <= stack)
			{
				break;
			} else
			{
				l = *--sp;
				r = *--sp;
			}
		}
		else
		{
			ix = l;
			jx = r;
			kx = l + ((int)(r - l) / 2);	/* kx is middle */
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
				} while (ix < r && (*ix)->addr < tval);
				do
				{
					jx--;
				} while (jx > l && (*jx)->addr > tval);
				t = *ix;
				*ix = *jx;
				*jx = t;
			} while (jx > ix);
			*jx = *ix;
			*ix = *r;
			*r = t;
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

void stpg_sort_new_contig(mstr_sort_array_element *base, mstr_sort_array_element *top)
{
	mstr_sort_array_element *stack[100], **sp;
	mstr_sort_array_element v, t;
	mstr_sort_array_element *l, *r;
	mstr_sort_array_element	*ix, *jx, *kx;
	char	*tval;

	sp = stack;
	l = base;
	r = top;
	for (;;)
	{
		if (r - l < S_CUTOFF)
		{
			for (ix  = l + 1;  ix <= r;  ix++)
			{
				for (jx = ix, t= *ix, tval = t.addr;  jx > l  &&  (jx - 1)->addr > tval;  jx--)
				{
					*jx = *(jx - 1);
				}
				*jx = t;
			}
			if (sp <= stack)
			{
				break;
			} else
			{
				l = *--sp;
				r = *--sp;
			}
		}
		else
		{
			ix = l;
			jx = r;
			kx = l + ((int)(r - l) / 2);	/* kx is middle */
			v = *kx;
			*kx = *jx;
			*jx = v;
			tval = v.addr;
			ix--;
			do
			{
				do
				{
					ix++;
				} while (ix < r && ix->addr < tval);
				do
				{
					jx--;
				} while (jx > l && jx->addr > tval);
				t = *ix;
				*ix = *jx;
				*jx = t;
			} while (jx > ix);
			*jx = *ix;
			*ix = *r;
			*r = t;
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

void stpg_sort_new(mstr_sort_array *array_p, size_t base, size_t top)
{
	ssize_t stack[100], *sp;
	mstr_sort_array_element v, t;
	ssize_t l, r;
	mstr_sort_array_element	*ix, *jx, *kx;
	ssize_t			ii, ji, ki;
	char	*tval;

	sp = stack;
	l = (ssize_t)base;
	r = (ssize_t)top;
	for (;;)
	{
		if (r - l < S_CUTOFF)
		{
			for (ii = l + 1; ii <= r; ii++)
			{
				ix = &array_p->array[REAL_SORT_INDEX(ii, array_p)];
				for (ji = ii, jx = ix, t = *ix, tval = t.addr;
						ji > l  &&  array_p->array[REAL_SORT_INDEX((ji - 1), array_p)].addr > tval;
						ji--, jx = &array_p->array[REAL_SORT_INDEX(ji, array_p)])
					*jx = array_p->array[REAL_SORT_INDEX((ji - 1), array_p)];
				*jx = t;
			}
			if (sp <= stack)
				break;
			l = *--sp;
			r = *--sp;
		} else
		{
			ii = l;
			ji = r;
			jx = &array_p->array[REAL_SORT_INDEX(ji, array_p)];
			ki = l + ((r - l) / 2);	/* ki is middle */
			kx = &array_p->array[REAL_SORT_INDEX(ki, array_p)];
			v = *kx;
			*kx = *jx;
			*jx = v;
			tval = v.addr;
			ii--;
			do
			{
				do
				{
					ii++;
					ix = &array_p->array[REAL_SORT_INDEX(ii, array_p)];
				} while (ii < r && ix->addr < tval);
				do
				{
					ji--;
					jx = &array_p->array[REAL_SORT_INDEX(ji, array_p)];
				} while (ji > l && jx->addr > tval);
				t = *ix;
				*ix = *jx;
				*jx = t;
			} while (ji > ii);
			*jx = *ix;
			jx = &array_p->array[REAL_SORT_INDEX(r, array_p)];
			*ix = *jx;
			*jx = t;
			if (ii - l > r - ii)
			{
				*sp++ = ii - 1;
				*sp++ = l;
				l = ii + 1;
			}
			else
			{
				*sp++ = r;
				*sp++ = ii + 1;
				r = ii - 1;
			}
		}
	}
	return;
}
