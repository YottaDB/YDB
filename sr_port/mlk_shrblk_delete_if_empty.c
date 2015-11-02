/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrblk_delete_if_empty.h"


bool	mlk_shrblk_delete_if_empty(mlk_ctldata_ptr_t ctl,
				   mlk_shrblk_ptr_t d)
{
	mlk_shrblk_ptr_t	r, l, p;
	mlk_shrsub_ptr_t	sub;


	if (d->children != 0  ||  d->owner != 0  ||  d->pending != 0)
		return FALSE;

	if (d->parent == 0)
		p = NULL;
	else
		p = (mlk_shrblk_ptr_t)R2A(d->parent);

	l = (mlk_shrblk_ptr_t)R2A(d->lsib);
	r = (mlk_shrblk_ptr_t)R2A(d->rsib);
	if (d == r)
		if (p == NULL)
			ctl->blkroot = 0;
		else
			p->children = 0;
	else
	{
		assert(d != l);
		A2R(r->lsib, l);
		A2R(l->rsib, r);
		if (p != NULL  &&  (mlk_shrblk_ptr_t)R2A(p->children) == d)
			A2R(p->children, r);
		else
			if ((mlk_shrblk_ptr_t)R2A(ctl->blkroot) == d)
				A2R(ctl->blkroot, r);
	}

	sub = (mlk_shrsub_ptr_t)R2A(d->value);
	PUT_ZERO(sub->backpointer);

	p = (mlk_shrblk_ptr_t)R2A(ctl->blkfree);
	memset(d, 0, SIZEOF(mlk_shrblk));
	A2R(d->rsib, p);

	A2R(ctl->blkfree, d);
	++ctl->blkcnt;

	return TRUE;

}

