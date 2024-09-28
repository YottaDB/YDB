/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
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
#include "have_crit.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	uint4		process_id;

void mlk_shrhash_delete(mlk_pvtctl_ptr_t ctl, mlk_shrblk_ptr_t d);

boolean_t mlk_shrblk_delete_if_empty(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t our)
{
	intrpt_state_t		prev_intrpt_state;
	mlk_shrblk_ptr_t	rght, lft, parent, child;
	mlk_shrsub_ptr_t	sub;
#	ifdef DEBUG
	ptroff_t		offset;
#	endif

	assert(LOCK_CRIT_HELD(pctl->csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	if (our->children != 0  ||  our->owner != 0  ||  our->pending != 0)
		return FALSE;
#	ifdef DEBUG
	A2R(offset, our);
	CHECK_SHRBLKPTR(offset, *pctl);
#	endif
	parent = (our->parent == 0) ? NULL : (mlk_shrblk_ptr_t)R2A(our->parent);
	if (parent)
		CHECK_SHRBLKPTR(our->parent, *pctl);
	assertpro((0 != our->lsib) && (0 != our->rsib));
	lft = (mlk_shrblk_ptr_t)R2A(our->lsib);
	rght = (mlk_shrblk_ptr_t)R2A(our->rsib);
	mlk_shrhash_delete(pctl, our);
	if (our == rght)
	{	/* If our == rght, it means our is the last element in the sibling circular list */
		if (NULL == parent)
			pctl->ctl->blkroot = 0;
		else
			parent->children = 0;
	} else
	{
		assert(our != lft);
		A2R(rght->lsib, lft);
		A2R(lft->rsib, rght);
		if ((NULL != parent) && ((mlk_shrblk_ptr_t)R2A(parent->children) == our))
		{
			A2R(parent->children, rght);
			if (lft == rght)
				assertpro((mlk_shrblk_ptr_t)R2A(parent->children) == lft);
		}
		if ((mlk_shrblk_ptr_t)R2A(pctl->ctl->blkroot) == our)
		{
			assertpro(NULL == parent);
			A2R(pctl->ctl->blkroot, rght);
		}
	}
	if (parent)
	{
		assertpro(!parent->children || ((mlk_shrblk_ptr_t)R2A(parent->children) != our));
		if (parent->children)
		{
			child = (mlk_shrblk_ptr_t)R2A(parent->children);
			assertpro((mlk_shrblk_ptr_t)R2A(child->parent) == parent);
		}
	}
	sub = (mlk_shrsub_ptr_t)R2A(our->value);
	sub->backpointer = 0;
	memset(our, 0, SIZEOF(mlk_shrblk));
	if (0 != pctl->ctl->blkfree)
	{
		parent = (mlk_shrblk_ptr_t)R2A(pctl->ctl->blkfree);
		A2R(our->rsib, parent);
	}
	our->lsib = INVALID_LSIB_MARKER;
	A2R(pctl->ctl->blkfree, our);
	++pctl->ctl->blkcnt;
	return TRUE;
}

void mlk_shrhash_delete(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t d)
{
	int			bi, si;
	mlk_shrblk_ptr_t	search_shrblk;
	mlk_shrhash_map_t	usedmap;
	mlk_shrhash_ptr_t	bucket, search_bucket, shrhash;
	mlk_subhash_res_t	hashres;
	mlk_subhash_state_t	hs;
	mlk_subhash_val_t	hash;
	uint4			num_buckets, total_len;

	assert(LOCK_CRIT_HELD(pctl->csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	shrhash = pctl->shrhash;
	num_buckets = pctl->shrhash_size;
#	ifdef DEBUG
	MLK_SUBHASH_INIT_PVTCTL(pctl, hs);
	total_len = 0;
	mlk_shrhash_val_build(d, &total_len, &hs);
	MLK_SUBHASH_FINALIZE(hs, total_len, hashres);
	hash = MLK_SUBHASH_RES_VAL(hashres);
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
}

/* Build the hash value for a shrblk by following parent pointers recursively and hashing the shrsubs from the top.
 * Keep in sync with MLK_PVTBLK_SUBHASH_GEN().
 *
 * Note: At some point we might consider adding a hash field to the shrblk to make this routine trivial at the cost of some space.
 *       We could make up for it by taking the hash value out of the shrhash at the cost of an extra R2A() and shrblk reference
 *       for each bucket comparison. For now the selected options seem reasonable.
 */
void mlk_shrhash_val_build(mlk_shrblk_ptr_t d, uint4 *total_len, mlk_subhash_state_t *hs)
{
	mlk_shrsub_ptr_t	shrsub;

	assert(INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state);
	assertpro(0 != d->value);
	if (d->parent)
		mlk_shrhash_val_build((mlk_shrblk_ptr_t)R2A(d->parent), total_len, hs);
	shrsub = (mlk_shrsub_ptr_t)R2A(d->value);
	*total_len += shrsub->length + 1;
	MLK_SUBHASH_INGEST(*hs, &shrsub->length, shrsub->length + 1);
}
