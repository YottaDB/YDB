/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stpg_sort.h"

#define S_CUTOFF 15

/*	stpg_sort sorts an array of pointers to mstr's using the value
	of each mstr's addr field as the key.  The algorithm is a modified
	QuickSort algorithm.
*/

void stpg_sort (mstr **base, mstr **top)
{
	mstr	**stack[50], ***sp;
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
			}
			else
			{
				l = *--sp;
				r = *--sp;
			}
		}
		else
		{
			ix = l;
			jx = r;
			kx = l + ((int)(r - l) / 2);
			kx = ((*ix)->addr > (*jx)->addr) ?
				(((*jx)->addr > (*kx)->addr) ?
					jx :
					(((*ix)->addr > (*kx)->addr) ? kx : ix)) :
				(((*jx)->addr < (*kx)->addr) ?
					jx :
					(((*ix)->addr > (*kx)->addr) ? ix : kx));
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
