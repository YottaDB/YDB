/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mvalconv.h"

#include <stddef.h>

#include "gtm_string.h"

#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrblk_create.h"
<<<<<<< HEAD
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsbgtr.h"

void mlk_shrhash_add(mlk_pvtblk *p, mlk_shrblk_ptr_t shr, int subnum);
=======
#include "mlk_shrhash_find_bucket.h"
#include "mlk_garbage_collect.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlk_ops.h"

#ifdef MLK_SHRHASH_DEBUG
#define SHRHASH_DEBUG_ONLY(x) x
#else
#define SHRHASH_DEBUG_ONLY(x)
#endif

boolean_t mlk_shrhash_add(mlk_pvtblk *p, mlk_shrblk_ptr_t shr, int subnum);
#ifdef MLK_SHRHASH_DEBUG
void mlk_shrhash_validate(mlk_ctldata_ptr_t ctl);
#endif
>>>>>>> 74ea4a3c... GT.M V6.3-006

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
<<<<<<< HEAD
	int			bi, fi, si, mi, bitnum;
=======
	int			bi, fi, si, mi, loop_cnt, tries = 0;
>>>>>>> 74ea4a3c... GT.M V6.3-006
	uint4			hash, num_buckets, usedmap;
	boolean_t		bucket_full;
	mlk_shrhash_ptr_t	shrhash, bucket, free_bucket, search_bucket, move_bucket;
	mlk_shrblk_ptr_t	move_shrblk;
	char			*str_ptr;
	mlk_shrsub_ptr_t	sub;

	SHRHASH_DEBUG_ONLY(mlk_shrhash_validate(p->ctlptr));
	shrhash = p->pvtctl.shrhash;
	num_buckets = p->pvtctl.shrhash_size;
	hash = MLK_PVTBLK_SUBHASH(p, subnum);
	bi = hash % num_buckets;
	bucket = &shrhash[bi];
<<<<<<< HEAD
	if (0 == bucket->shrblk)
=======
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
>>>>>>> 74ea4a3c... GT.M V6.3-006
	{	/* Target bucket is free, so just use it. */
		assert(!IS_NEIGHBOR(bucket->usedmap, 0));
		bucket->shrblk_idx = MLK_SHRBLK_IDX(p->pvtctl, shr);
		assert(0 < bucket->shrblk_idx);
		bucket->hash = hash;
		SET_NEIGHBOR(bucket->usedmap, 0);
		SHRHASH_DEBUG_ONLY(mlk_shrhash_validate(p->ctlptr));
<<<<<<< HEAD
		return;
	}
	/* Search for free bucket */
	assert(p->ctlptr->blkcnt < num_buckets);
	for (fi = (bi + 1) % num_buckets ; 0 != shrhash[fi].shrblk ; fi = (fi + 1) % num_buckets)
	{
		assert(fi != bi);	/* Table full not possible because of prior assert about "p->ctlptr->blkcnt" greater
					 * than p->ctlptr->num_blkhash, i.e. we allocated more mlk_shrhash structures
					 * than mlk_shrsub structures in "mlk_shr_init".
					 */
	}
	bucket_full = FALSE;
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
		if (si == fi)
		{	/* No movable buckets. Normally one needs to resize the hash table but since this hash
			 * table is in shared memory, it cannot be resized (needs shared memory size change which
			 * is not easily possible). Degenerate to linear search scheme for just this bucket.
			 */
			sgmnt_addrs	*csa;

			bucket_full = TRUE;
			csa = &FILE_INFO(p->region)->s_addrs;
			/* Record this rare event in the file header */
			BG_TRACE_PRO_ANY(csa, lock_hash_bucket_full);
			break;
		}
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
	/* We found one free bucket (mostly close enough, but rarely not close enough). In either case store the new data there */
	free_bucket = &shrhash[fi];
	assert(0 == free_bucket->shrblk);
	A2R(free_bucket->shrblk, shr);
	assert(0 < free_bucket->shrblk);
	free_bucket->hash = hash;
	if (!bucket_full)
	{
		bitnum = (num_buckets + fi - bi) % num_buckets;
		assert(MLK_SHRHASH_NEIGHBORS > bitnum);
		assert(0 == (bucket->usedmap & (1U << bitnum)));
			/* Assert before adding that neighbor bucket was not used before */
	} else
		bitnum = MLK_SHRHASH_HIGHBIT;
	/* Note the new neighbor of the original bucket */
	bucket->usedmap |= (1U << bitnum);
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
	for (bi = 0; bi < num_buckets; bi++)
	{
		bucket = &shrhash[bi];
		usedmap = bucket->usedmap;
		usedmap = usedmap & ~(1U << MLK_SHRHASH_HIGHBIT);
		for (ni = bi; usedmap; (usedmap >>= 1), (ni = (ni + 1) % num_buckets))
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
			original_bucket = &shrhash[obi];
			if (MLK_SHRHASH_NEIGHBORS > ((num_buckets + bi - obi) % num_buckets))
				assert(0 != (original_bucket->usedmap & (1U << ((num_buckets + bi - obi) % num_buckets))));
			else
				assert(original_bucket->usedmap & (1U << MLK_SHRHASH_HIGHBIT));
		}
	}
}

#endif
=======
		return TRUE;
	}
	fi = mlk_shrhash_find_bucket(&p->pvtctl, hash);
	if (fi == -1)
		return FALSE;
	/* We found one close enough, so store the new data there */
	mlk_shrhash_insert(&p->pvtctl, bi, fi, MLK_SHRBLK_IDX(p->pvtctl, shr), hash);
	return TRUE;
}
>>>>>>> 74ea4a3c... GT.M V6.3-006
