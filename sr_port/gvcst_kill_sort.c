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
#include "gdsroot.h"
#include "gdskill.h"
#include "gvcst_kill_sort.h"

#define S_CUTOFF 15

void gvcst_kill_sort(kill_set *k)
{

	block_id_ptr_t	stack[50],*sp;
	block_id	v,t;
	block_id_ptr_t	l,r;
	block_id_ptr_t	ix,jx,kx;

	assert(k->used <= BLKS_IN_KILL_SET);
	sp = stack;
	l = (block_id_ptr_t)(k->blk);
	r = l + k->used-1;
	for (;;)
		if (r - l < S_CUTOFF)
		{
			for (ix  = l + 1 ; ix <= r ; ix++)
			{
				for (jx = ix , t = *ix; jx > l  && *(jx-1) > t; jx--)
					*jx = *(jx - 1);
				*jx = t;
			}
			if (sp <= stack)
				break;
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
			kx = (*ix > *jx) ?
				((*jx > *kx) ?
					jx:
					((*ix > *kx) ? kx : ix)):
				((*jx < *kx) ?
					jx:
					((*ix > *kx) ? ix : kx));
			t = *kx;
			*kx = *jx;
			*jx = t;
			ix--;
			do
			{
				do ix++; while (*ix < t);
				do jx--; while (*jx > t);
				v = *ix;
				*ix = *jx;
				*jx = v;
			} while (jx > ix);
			*jx = *ix;
			*ix = *r;
			*r = v;
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
	return;

}
