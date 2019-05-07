/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "mmrhash.h"

void mlk_shrhash_delete(mlk_pvtctl_ptr_t ctl, mlk_shrblk_ptr_t d);
void mlk_shrhash_val_build(mlk_shrblk_ptr_t d, uint4 *total_len, hash128_state_t *hs);

boolean_t mlk_shrblk_delete_if_empty(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t d)
{
	mlk_shrblk_ptr_t	r, l, p, child;
	mlk_shrsub_ptr_t	sub;
#	ifdef DEBUG
	ptroff_t		offset;
#	endif

	if (d->children != 0  ||  d->owner != 0  ||  d->pending != 0)
		return FALSE;
#	ifdef DEBUG
	A2R(offset, d);
	CHECK_SHRBLKPTR(offset, *pctl);
#	endif
	p = (d->parent == 0) ? NULL : (mlk_shrblk_ptr_t)R2A(d->parent);
	if (p)
		CHECK_SHRBLKPTR(d->parent, *pctl);
	assert((0 != d->lsib) && (0 != d->rsib));
	l = (mlk_shrblk_ptr_t)R2A(d->lsib);
	r = (mlk_shrblk_ptr_t)R2A(d->rsib);
	mlk_shrhash_delete(pctl, d);
	if (d == r)
	{	/* If d == r, it means d is the last element in the sibling circular list */
		if (p == NULL)
			pctl->ctl->blkroot = 0;
		else
			p->children = 0;
	} else
	{
		assert(d != l);
		A2R(r->lsib, l);
		A2R(l->rsib, r);
		if ((p != NULL) && (mlk_shrblk_ptr_t)R2A(p->children) == d)
		{
			A2R(p->children, r);
			assert((l != r) || ((mlk_shrblk_ptr_t)R2A(p->children) == l));
		}
		if ((mlk_shrblk_ptr_t)R2A(pctl->ctl->blkroot) == d)
		{
			assert(NULL == p);
			A2R(pctl->ctl->blkroot, r);
		}
	}
	if (p)
	{
		assert(!p->children || ((mlk_shrblk_ptr_t)R2A(p->children) != d));
		if (p->children)
		{
			child = (mlk_shrblk_ptr_t)R2A(p->children);
			assert((mlk_shrblk_ptr_t)R2A(child->parent) == p);
		}
	}
	sub = (mlk_shrsub_ptr_t)R2A(d->value);
	sub->backpointer = 0;
	memset(d, 0, SIZEOF(mlk_shrblk));
	if (0 != pctl->ctl->blkfree)
	{
		p = (mlk_shrblk_ptr_t)R2A(pctl->ctl->blkfree);
		A2R(d->rsib, p);
	}
	d->lsib = INVALID_LSIB_MARKER;
	A2R(pctl->ctl->blkfree, d);
	++pctl->ctl->blkcnt;
	return TRUE;
}

void mlk_shrhash_delete(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t d)
{
	hash128_state_t		hs;
	gtm_uint16		hashres;
	uint4			hash, total_len, num_buckets;
	mlk_shrhash_map_t	usedmap;
	mlk_shrblk_ptr_t	search_shrblk;
	int			bi, si, bitnum;
	mlk_shrhash_ptr_t	shrhash, bucket, search_bucket;

	shrhash = pctl->shrhash;
	num_buckets = pctl->shrhash_size;
#	ifdef DEBUG
	HASH128_STATE_INIT(hs, 0);
	total_len = 0;
	mlk_shrhash_val_build(d, &total_len, &hs);
	ydb_mmrhash_128_result(&hs, total_len, &hashres);
	DBG_LOCKHASH_N_BITS(hashres.one);
	hash = (uint4)hashres.one;
	assert(hash == d->hash);
#	else
	hash = d->hash;
#	endif
	bi = hash % num_buckets;
	bucket = &shrhash[bi];
	usedmap = bucket->usedmap;
	for (si = bi ; 0 != usedmap ; (si = (si + 1) % num_buckets), (usedmap >>= 1))
	{
		if (0 == (usedmap & 1U))
			continue;
		search_bucket = &shrhash[si];
		if (search_bucket->hash != hash)
			continue;
		assert(0 != search_bucket->shrblk_idx);
		search_shrblk = MLK_SHRHASH_SHRBLK(*pctl, search_bucket);
		if (d == search_shrblk)
		{
			assert(IS_NEIGHBOR(bucket->usedmap, (num_buckets + si - bi) % num_buckets));
			search_bucket->shrblk_idx = 0;
			search_bucket->hash = 0;
			CLEAR_NEIGHBOR(bucket->usedmap, (num_buckets + si - bi) % num_buckets);
			return;
		}
	}
	assert(usedmap);
	return;
}

/* Build the hash value for a shrblk by following parent pointers recursively and hashing the shrsubs from the top.
 * Keep in sync with MLK_PVTBLK_SUBHASH_GEN().
 *
 * Note: At some point we might consider adding a hash field to the shrblk to make this routine trivial at the cost of some space.
 *       We could make up for it by taking the hash value out of the shrhash at the cost of an extra R2A() and shrblk reference
 *       for each bucket comparison. For now the selected options seem reasonable.
 */
void mlk_shrhash_val_build(mlk_shrblk_ptr_t d, uint4 *total_len, hash128_state_t *hs)
{
	mlk_shrsub_ptr_t	shrsub;

	assert(0 != d->value);
	if (d->parent)
		mlk_shrhash_val_build((mlk_shrblk_ptr_t)R2A(d->parent), total_len, hs);
	shrsub = (mlk_shrsub_ptr_t)R2A(d->value);
	*total_len += shrsub->length + 1;
	ydb_mmrhash_128_ingest(hs, &shrsub->length, shrsub->length + 1);
}
