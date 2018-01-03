/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#ifdef MLK_SHRHASH_DEBUG
#define SHRHASH_DEBUG_ONLY(x) x
#else
#define SHRHASH_DEBUG_ONLY(x)
#endif

void mlk_shrhash_add(mlk_pvtblk *p, mlk_shrblk_ptr_t shr, int subnum);
#ifdef MLK_SHRHASH_DEBUG
void mlk_shrhash_validate(mlk_ctldata_ptr_t ctl);
#endif

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
	mlk_shrhash_add(p, ret, p->subscript_cnt - nshrs);
	return ret;
}

void mlk_shrhash_add(mlk_pvtblk *p, mlk_shrblk_ptr_t shr, int subnum)
{
	int			bi, fi, si, mi;
	uint4			hash, num_buckets, usedmap;
	mlk_shrhash_ptr_t	shrhash, bucket, free_bucket, search_bucket, move_bucket;
	mlk_shrblk_ptr_t	move_shrblk;

	SHRHASH_DEBUG_ONLY(mlk_shrhash_validate(p->ctlptr));
	shrhash = (mlk_shrhash_ptr_t)R2A(p->ctlptr->blkhash);
	num_buckets = p->ctlptr->num_blkhash;
	hash = MLK_PVTBLK_SUBHASH(p, subnum);
	bi = hash % num_buckets;
	bucket = &shrhash[bi];
	assert(MAXUINT4 > bucket->usedmap);
	if (0 == bucket->shrblk)
	{	/* Target bucket is free, so just use it. */
		assert(0 == (bucket->usedmap & 1U));
		A2R(bucket->shrblk, shr);
		assert(0 < bucket->shrblk);
		bucket->hash = hash;
		bucket->usedmap |= 1U;
		SHRHASH_DEBUG_ONLY(mlk_shrhash_validate(p->ctlptr));
		return;
	}
	/* Search for free bucket */
	for (fi = (bi + 1) % num_buckets ; 0 != shrhash[fi].shrblk ; fi = (fi + 1) % num_buckets)
		assert(fi != bi);	/* Table full */
	/* While free bucket is out of the neighborhood, find a closer one that can be moved into it. */
	while (MLK_SHRHASH_NEIGHBORS <= ((num_buckets + fi - bi) % num_buckets))
	{	/* Find a bucket with a neighbor which can be moved to the free bucket */
		for ((si = ((num_buckets + fi - MLK_SHRHASH_NEIGHBORS + 1) % num_buckets)), (mi = fi) ;
				si != fi ;
				si = (si + 1) % num_buckets)
		{
			search_bucket = &shrhash[si];
			usedmap = search_bucket->usedmap;
			if (0 == usedmap)
				continue;
			/* Pull the earliest used bucket out of the map, but don't pass the free bucket. */
			for (mi = si ; (0 == (usedmap & 1U)) && (mi != fi) ; usedmap >>= 1)
			{
				assert(0 != usedmap);
				mi = (mi + 1) % num_buckets;
			}
			if (mi != fi)
				break;
		}
		assert(si != fi);	/* Otherwise no movable buckets */
		/* Move the bucket from the mapped bucket to the free bucket */
		move_bucket = &shrhash[mi];
		free_bucket = &shrhash[fi];
		assert(0 != move_bucket->shrblk);
		assert(0 == free_bucket->shrblk);
		/* Check that moving neighbor was marked used and free neighbor was not */
		assert(0 != (search_bucket->usedmap & (1U << ((num_buckets + mi - si) % num_buckets))));
		assert(0 == (search_bucket->usedmap & (1U << ((num_buckets + fi - si) % num_buckets))));
		/* We are moving a relative pointer, so convert to absolute and back */
		move_shrblk = (mlk_shrblk_ptr_t)R2A(move_bucket->shrblk);
		A2R(free_bucket->shrblk, move_shrblk);
		assert(0 < free_bucket->shrblk);
		assert(MLK_SHRHASH_NEIGHBORS > ((num_buckets + fi - (move_bucket->hash % num_buckets)) % num_buckets));
		free_bucket->hash = move_bucket->hash;
		move_bucket->shrblk = 0;
		move_bucket->hash = 0;
		/* Clear bit for moved neighbor, set bit for target neighbor */
		search_bucket->usedmap &= ~(1U << ((num_buckets + mi - si) % num_buckets));
		search_bucket->usedmap |= (1U << ((num_buckets + fi - si) % num_buckets));
		/* The moved neighbor is now free */
		fi = mi;
	}
	/* We found one close enough, so store the new data there */
	assert(0 == (bucket->usedmap & (1U << ((num_buckets + fi - bi) % num_buckets))));	/* Neighbor was not used before */
	free_bucket = &shrhash[fi];
	assert(0 == free_bucket->shrblk);
	A2R(free_bucket->shrblk, shr);
	assert(0 < free_bucket->shrblk);
	assert(MLK_SHRHASH_NEIGHBORS > ((num_buckets + fi - (hash % num_buckets)) % num_buckets));
	free_bucket->hash = hash;
	/* Note the new neighbor of the original bucket */
	bucket->usedmap |= (1U << ((num_buckets + fi - bi) % num_buckets));
	SHRHASH_DEBUG_ONLY(mlk_shrhash_validate(p->ctlptr));
}

#ifdef MLK_SHRHASH_DEBUG

void mlk_shrhash_validate(mlk_ctldata_ptr_t ctl)
{
	mlk_shrhash_ptr_t	shrhash, bucket, neighbor_bucket, original_bucket;
	uint4			num_buckets, usedmap;
	int			bi, ni, obi;

	shrhash = (mlk_shrhash_ptr_t)R2A(ctl->blkhash);
	num_buckets = ctl->num_blkhash;
	for (bi=0; bi < num_buckets; bi++)
	{
		bucket = &shrhash[bi];
		for ((usedmap = bucket->usedmap), (ni = bi); usedmap; (usedmap >>= 1), (ni = (ni + 1) % num_buckets))
		{
			if (usedmap & 1U)
			{
				neighbor_bucket = &shrhash[ni];
				assert(0 != neighbor_bucket->shrblk);
				assert(neighbor_bucket->hash % num_buckets == bi);
			}
		}
		if (0 == bucket->shrblk)
			assert(0 == (usedmap & 1U));
		else
		{
			obi = bucket->hash % num_buckets;
			assert(MLK_SHRHASH_NEIGHBORS > ((num_buckets + bi - obi) % num_buckets));
			original_bucket = &shrhash[obi];
			assert(0 != (original_bucket->usedmap & (1U << ((num_buckets + bi - obi) % num_buckets))));
		}
	}
}

#endif
