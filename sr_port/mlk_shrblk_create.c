/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stddef.h>
#include "mdef.h"
#include "mvalconv.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrblk_create.h"
#include "mlk_shrhash_find_bucket.h"
#include "mlk_garbage_collect.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlk_ops.h"
#include "mlk_shrhash_add.h"

#ifdef MLK_SHRHASH_DEBUG
#define SHRHASH_DEBUG_ONLY(x) x
#else
#define SHRHASH_DEBUG_ONLY(x)
#endif

#ifdef MLK_SHRHASH_DEBUG
void mlk_shrhash_validate(mlk_ctldata_ptr_t ctl);
#endif

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
	assert(p->hash_seed == ctl->hash_seed);
	ret->hash = MLK_PVTBLK_SUBHASH(p, p->subscript_cnt - nshrs);
	if (mlk_shrhash_add(&p->pvtctl, ret))
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

boolean_t mlk_shrhash_add(mlk_pvtctl *pctl, mlk_shrblk_ptr_t shr)
{
	int			bi, fi, si, mi, loop_cnt, tries = 0;
	uint4			num_buckets;
	char			*str_ptr;
	mlk_shrhash_ptr_t	shrhash, bucket, free_bucket, search_bucket, move_bucket;
	mlk_shrblk_ptr_t	move_shrblk;
	mlk_shrsub_ptr_t	sub;
	mlk_shrhash_map_t	usedmap;
	mlk_subhash_val_t	hash;

	SHRHASH_DEBUG_ONLY(mlk_shrhash_validate(p->ctlptr));
	shrhash = pctl->shrhash;
	num_buckets = pctl->shrhash_size;
	hash = shr->hash;
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
		bucket->shrblk_idx = MLK_SHRBLK_IDX(*pctl, shr);
		assert(0 < bucket->shrblk_idx);
		bucket->hash = hash;
		SET_NEIGHBOR(bucket->usedmap, 0);
		SHRHASH_DEBUG_ONLY(mlk_shrhash_validate(p->ctlptr));
		return TRUE;
	}
	fi = mlk_shrhash_find_bucket(pctl, hash);
	if (MLK_SHRHASH_FOUND_NO_BUCKET == fi)
		return FALSE;
	/* We found one close enough, so store the new data there */
	mlk_shrhash_insert(pctl, bi, fi, MLK_SHRBLK_IDX(*pctl, shr), hash);
	return TRUE;
}
