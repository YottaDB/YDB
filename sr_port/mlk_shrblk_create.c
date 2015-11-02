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

	/* I *believe* the purpose of this test is to tell if some other process has stolen the space available
	   and made it impossible for us to fit all of our subscripts into shrsubs. I have added the "3" to the
	   size of the shrsub since this is the maximum rounding that would be added for word alignment. It is
	   not exact and could potentially refuse a lock that would fail were the value exact but if a user is
	   running that close to the edge, they should be increasing their lock space or using fewer locks anyway.
	   (see 7/97).
        */
	n = (ptroff_t)(nshrs * (3 + SIZEOF(mlk_shrsub) - 1) + (p->total_length - (val - p->value)));
	if (ctl->subtop - ctl->subfree < n || ctl->blkcnt < nshrs)
		return 0;

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
	n = (ptroff_t)ROUND_UP(SIZEOF(mlk_shrsub) - 1 + len, SIZEOF(ptroff_t));
	if (ctl->subtop - ctl->subfree < n)
		GTMASSERT;
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
