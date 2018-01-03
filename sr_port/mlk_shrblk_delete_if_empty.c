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

#include "mdef.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mmrhash.h"

void mlk_shrhash_delete(mlk_ctldata_ptr_t ctl, mlk_shrblk_ptr_t d);
void mlk_shrhash_val_build(mlk_shrblk_ptr_t d, uint4 *total_len, hash128_state_t *hs);

boolean_t mlk_shrblk_delete_if_empty(mlk_ctldata_ptr_t ctl, mlk_shrblk_ptr_t d)
{
	mlk_shrblk_ptr_t	r, l, p;
	mlk_shrsub_ptr_t	sub;

	if (d->children != 0  ||  d->owner != 0  ||  d->pending != 0)
		return FALSE;
	p = (d->parent == 0) ? NULL : (mlk_shrblk_ptr_t)R2A(d->parent);
	l = (mlk_shrblk_ptr_t)R2A(d->lsib);
	r = (mlk_shrblk_ptr_t)R2A(d->rsib);
	mlk_shrhash_delete(ctl, d);
	if (d == r)
	{
		if (p == NULL)
			ctl->blkroot = 0;
		else
			p->children = 0;
	} else
	{
		assert(d != l);
		A2R(r->lsib, l);
		A2R(l->rsib, r);
		if (p != NULL  &&  (mlk_shrblk_ptr_t)R2A(p->children) == d)
			A2R(p->children, r);
		else if ((mlk_shrblk_ptr_t)R2A(ctl->blkroot) == d)
			A2R(ctl->blkroot, r);
	}
	sub = (mlk_shrsub_ptr_t)R2A(d->value);
	sub->backpointer = 0;
	p = (mlk_shrblk_ptr_t)R2A(ctl->blkfree);
	memset(d, 0, SIZEOF(mlk_shrblk));
	A2R(d->rsib, p);
	A2R(ctl->blkfree, d);
	++ctl->blkcnt;
	return TRUE;
}

void mlk_shrhash_delete(mlk_ctldata_ptr_t ctl, mlk_shrblk_ptr_t d)
{
	hash128_state_t		hs;
	gtm_uint16		hashres;
	uint4			hash, total_len = 0, num_buckets, usedmap;
	mlk_shrblk_ptr_t	search_shrblk;
	int			bi, si;
	mlk_shrhash_ptr_t	shrhash, bucket, search_bucket;

	shrhash = (mlk_shrhash_ptr_t)R2A(ctl->blkhash);
	num_buckets = ctl->num_blkhash;
	HASH128_STATE_INIT(hs, 0);
	mlk_shrhash_val_build(d, &total_len, &hs);
	gtmmrhash_128_result(&hs, total_len, &hashres);
	hash = (uint4)hashres.one;
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
		assert(0 != search_bucket->shrblk);
		search_shrblk = (mlk_shrblk_ptr_t)R2A(search_bucket->shrblk);
		if (d == search_shrblk)
		{
			assert(0 != (bucket->usedmap & (1U << ((num_buckets + si - bi) % num_buckets))));
			search_bucket->shrblk = 0;
			search_bucket->hash = 0;
			bucket->usedmap &= ~(1U << ((num_buckets + si - bi) % num_buckets));
			return;
		}
	}
	assert(usedmap);
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

	if (d->parent)
		mlk_shrhash_val_build((mlk_shrblk_ptr_t)R2A(d->parent), total_len, hs);
	shrsub = (mlk_shrsub_ptr_t)R2A(d->value);
	*total_len += shrsub->length + 1;
	gtmmrhash_128_ingest(hs, &shrsub->length, shrsub->length + 1);
}
