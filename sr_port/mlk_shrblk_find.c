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
#include "mlkdef.h"
#include "mmemory.h"
#include "is_proc_alive.h"
#include "mlk_shrblk_find.h"
#include "mlk_shrblk_create.h"

GBLREF int4 process_id;

bool mlk_find_blocking_child_lock(mlk_pvtblk *p, mlk_shrblk_ptr_t child, UINTPTR_T auxown);

bool mlk_find_blocking_child_lock(mlk_pvtblk *p, mlk_shrblk_ptr_t child, UINTPTR_T auxown)
{
	mlk_shrblk_ptr_t	d, dhead, d1;
	bool			blocked;

	blocked = FALSE;

	for (dhead = d = child, d1 = NULL ; dhead != d1 && !blocked ; d = d1 = (mlk_shrblk_ptr_t)R2A(d->rsib))
	{
		/* There is similar code below to check if the
		 * process that owns the lock still exists. Any change in this
		 * part of the code should be propogated to those segments
		 * too.
		 */
		if (d->owner && (d->owner != process_id || d->auxowner != auxown))
		{	/* If owned and not owned by us check if owner is alive */
			if (is_proc_alive(d->owner, d->image_count))
			{	/* Signal that this lock request is blocked by this node */
				p->blocked = d;
				p->blk_sequence = d->sequence;
				blocked = TRUE;
			} else
			{	/* Owner is dead so release this node */
				d->owner = 0;
				d->auxowner = 0;
			}
		}
		if (!blocked && d->children)
			blocked = mlk_find_blocking_child_lock(p, (mlk_shrblk_ptr_t)R2A(d->children), auxown);
	}

	return blocked;
}

bool mlk_shrblk_find(mlk_pvtblk       *p,
		     mlk_shrblk_ptr_t *ret,
		     UINTPTR_T        auxown)
{
	mlk_shrblk_ptr_t	pnt, d, d0, d1, dhead;
        ptroff_t		*chld_of_pnt, *cop1;
	mlk_shrsub_ptr_t	dsub;
	int			i, j, slen;
	unsigned char		*cp;
	bool			blocked;

	blocked = FALSE;
	*ret = 0;
	for (pnt = 0 , chld_of_pnt = (ptroff_t *)&p->ctlptr->blkroot , i = p->subscript_cnt , cp = p->value ;
		i > 0 ; i-- , pnt = d , chld_of_pnt = (ptroff_t *)&d->children, cp += slen)
	{
		slen = *cp++;

		if (!*chld_of_pnt)
		{
			if (!(d = mlk_shrblk_create(p, cp, slen, pnt, chld_of_pnt, i)))
			{
				return TRUE;
			}
			A2R(d->lsib, d);
			A2R(d->rsib, d);
		} else
		{
			for (d = (mlk_shrblk_ptr_t)R2A(*chld_of_pnt), dhead = d , cop1 = 0; ; d = (mlk_shrblk_ptr_t)R2A(d->rsib))
			{
				dsub = (mlk_shrsub_ptr_t)R2A(d->value);
				j = memvcmp(cp, slen, dsub->data, dsub->length);
				if (!j)
				{	/* We found the right node */
					if (d->owner && (d->owner != process_id || d->auxowner != auxown))
					{	/* If owned and not owned by us check if owner is alive */
						if (is_proc_alive(d->owner, d->image_count))
						{	/* Signal that this lock request is blocked by this node */
							p->blocked = d;
							p->blk_sequence = d->sequence;
							blocked = TRUE;
						} else
						{	/* Owner is dead so release this node */
							d->owner = 0;
							d->auxowner = 0;
						}
					}
					break;
				}
				if (j < 0)
				{	/* Insert new sibling to left of existing sibling */
					if (d == dhead)	/* New entry will be first in list */
						cop1 = chld_of_pnt;
					d0 = (mlk_shrblk_ptr_t)R2A(d->lsib);
					d1 = d;
					if (!(d = mlk_shrblk_create(p, cp, slen, pnt, cop1, i)))
					{
						return TRUE;	/* resource starve -- no room for new shrblk */
					}
					A2R(d->lsib, d0);
					A2R(d->rsib, d1);
					A2R(d0->rsib, d);
					A2R(d1->lsib, d);
					break;
				} else if ((mlk_shrblk_ptr_t)R2A(d->rsib) == dhead)
				{	/* Insert new sibling to right of existing sibling */
					d1 = (mlk_shrblk_ptr_t)R2A(d->rsib);
					d0 = d;
					if (!(d = mlk_shrblk_create(p, cp, slen, pnt, cop1, i)))
					{
						return TRUE;	/* resource starve -- no room for new shrblk */
					}
					A2R(d->lsib, d0);
					A2R(d->rsib, d1);
					A2R(d0->rsib, d);
					A2R(d1->lsib, d);
					break;
				}
			}
		}
		/* When we get to the last "subscript", it's node is our lock target */
		if (i == 1)
			*ret = d;
	}

	if (*chld_of_pnt && !blocked)
	{
		/* look at the subtree owners to see if we will be blocked by someone underneath */
		blocked = mlk_find_blocking_child_lock(p, (mlk_shrblk_ptr_t)R2A(*chld_of_pnt), auxown);
	}

	return blocked;
}
