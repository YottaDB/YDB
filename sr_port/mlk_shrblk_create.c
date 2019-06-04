/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>

#include "gtm_string.h"

#include "mvalconv.h"
#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrblk_create.h"
#include "mlk_shrhash_find_bucket.h"
#include "mlk_garbage_collect.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlk_ops.h"

boolean_t mlk_shrhash_add(mlk_pvtblk *p, mlk_shrblk_ptr_t shr, int subnum);

#define MAX_TRIES 4

mlk_shrblk_ptr_t mlk_shrblk_create(mlk_pvtblk *p,
				   unsigned char *val,		/* the subscript */
				   int len,			/* subscript's length */
				   mlk_shrblk_ptr_t par,	/* pointer to the parent (zero if top level) */
				   ptroff_t *ptr,		/* parent's pointer to us (zero if we are not the eldest child) */
				   int nshrs)			/* number of shrblks remaining to be created for this operation */
{
	mlk_ctldata_ptr_t	ctl;
	mlk_shrblk_ptr_t	ret, shr1;
	mlk_shrsub_ptr_t	subptr;
	ptroff_t		n;

	ctl = p->pvtctl.ctl;
	if ((ctl->subtop - ctl->subfree) < (MLK_PVTBLK_SHRSUB_SIZE(p, nshrs) - (val - p->value)) || ctl->blkcnt < nshrs)
		return NULL; /* There is not enough substring or shared block space */
	CHECK_SHRBLKPTR(ctl->blkfree, p->pvtctl);
	assert(ctl->blkfree != 0);
	ret = (mlk_shrblk_ptr_t)R2A(ctl->blkfree);
	ctl->blkcnt--;
	CHECK_SHRBLKPTR(ret->rsib, p->pvtctl);
	if (ret->rsib == 0)
		ctl->blkfree = 0;
	else
	{
		shr1 = (mlk_shrblk_ptr_t)R2A(ret->rsib);
		shr1->lsib = 0;
		A2R(ctl->blkfree, shr1);
		CHECK_SHRBLKPTR(ctl->blkfree, p->pvtctl);
		CHECK_SHRBLKPTR(shr1->rsib, p->pvtctl);
	}
	memset(ret, 0, SIZEOF(*ret));
	if (par)
		A2R(ret->parent, par);
	else
		ret->parent = 0;
	if (ptr)
	{
		assert(0 == *ptr);
		A2R(*ptr, ret);
	}
	n = (ptroff_t)ROUND_UP(OFFSETOF(mlk_shrsub, data[0]) + len, SIZEOF(ptroff_t));
	subptr = (mlk_shrsub_ptr_t)R2A(ctl->subfree);
	ctl->subfree += n;
	A2R(ret->value, subptr);
	A2R(subptr->backpointer, ret);
	assert(subptr->backpointer < 0);
	subptr->length = len;
	memcpy(subptr->data, val, len);
	ret->hash = MLK_PVTBLK_SUBHASH(p, p->subscript_cnt - nshrs);
	if (mlk_shrhash_add(p, ret, p->subscript_cnt - nshrs))
		return ret;
	/* We failed to add the block; return the shrblk and shrsub to the free lists */
	memset(subptr, 0, SIZEOF(mlk_shrsub));
	ctl->subfree -= n;
	memset(ret, 0, SIZEOF(mlk_shrblk));
	if (ctl->blkfree)
		A2R(ret->rsib, R2A(ctl->blkfree));
	else
		ret->rsib = 0;
	A2R(ctl->blkfree, ret);
	if (ptr)
		*ptr = 0;
	ctl->blkcnt++;
	return NULL;
}

boolean_t mlk_shrhash_add(mlk_pvtblk *p, mlk_shrblk_ptr_t shr, int subnum)
{
	int			bi, fi;
	uint4			hash, num_buckets;
	mlk_shrhash_ptr_t	shrhash, bucket;

	shrhash = p->pvtctl.shrhash;
	num_buckets = p->pvtctl.shrhash_size;
	hash = MLK_PVTBLK_SUBHASH(p, subnum);
	bi = hash % num_buckets;
	bucket = &shrhash[bi];
	assert(MLK_SHRHASH_MAP_MAX >= bucket->usedmap);
#	ifdef DEBUG
	if (WBTEST_ENABLED(WBTEST_LOCK_HASH_ZTW))
	{
		GBLREF mval dollar_ztwormhole;

		i2mval(&dollar_ztwormhole, bi);
		MV_FORCE_STRD(&dollar_ztwormhole);
	}
#	endif
	if (0 == bucket->shrblk_idx)
	{	/* Target bucket is free, so just use it. */
		assert(!IS_NEIGHBOR(bucket->usedmap, 0));
		bucket->shrblk_idx = MLK_SHRBLK_IDX(p->pvtctl, shr);
		assert(0 < bucket->shrblk_idx);
		bucket->hash = hash;
		SET_NEIGHBOR(bucket->usedmap, 0);
		return TRUE;
	}
	fi = mlk_shrhash_find_bucket(&p->pvtctl, hash);
	if (MLK_SHRHASH_FOUND_NO_BUCKET == fi)
		return FALSE;
	/* We found one close enough, so store the new data there */
	mlk_shrhash_insert(&p->pvtctl, bi, fi, MLK_SHRBLK_IDX(p->pvtctl, shr), hash);
	return TRUE;
}
