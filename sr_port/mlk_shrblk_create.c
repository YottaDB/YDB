/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stddef.h>
#include "mdef.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrblk_create.h"

mlk_shrblk_ptr_t mlk_shrblk_create(mlk_pvtblk *p,
				   unsigned char *val,		/* the subscript */
				   int len,			/* subscript's length */
				   mlk_shrblk_ptr_t par,	/* pointer to the parent (zero if top level) */
				   ptroff_t *ptr,		/* parent's pointer to us (zero if we are not the eldest child */
				   int nshrs)			/* number of shrblks remaining to be created for this operation */
{
	mlk_ctldata_ptr_t	ctl;
	mlk_shrblk_ptr_t	ret, shr1;
	mlk_shrsub_ptr_t	subptr;
	ptroff_t		n;

	ctl = p->ctlptr;
	if ((ctl->subtop - ctl->subfree) < (MLK_PVTBLK_SHRSUB_SIZE(p, nshrs) - (val - p->value)) || ctl->blkcnt < nshrs)
		return NULL; /* There is not enough substring or shared block space */
	ret = (mlk_shrblk_ptr_t)R2A(ctl->blkfree);
	ctl->blkcnt--;
	if (ret->rsib == 0)
		ctl->blkfree = 0;
	else
	{
		shr1 = (mlk_shrblk_ptr_t)R2A(ret->rsib);
		A2R(ctl->blkfree, shr1);
	}
	memset(ret, 0, SIZEOF(*ret));
	if (par)
		A2R(ret->parent, par);
	if (ptr)
		A2R(*ptr, ret);
	n = (ptroff_t)ROUND_UP(OFFSETOF(mlk_shrsub, data[0]) + len, SIZEOF(ptroff_t));
	subptr = (mlk_shrsub_ptr_t)R2A(ctl->subfree);
	ctl->subfree += n;
	A2R(ret->value, subptr);
	n = (ptroff_t)((sm_uc_ptr_t)ret - (sm_uc_ptr_t)&subptr->backpointer);
	assert (n < 0);
	subptr->backpointer = n;
	subptr->length = len;
	memcpy(subptr->data, val, len);
	return ret;
}
