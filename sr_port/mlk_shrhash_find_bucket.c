/****************************************************************
 *								*
 * Copyright (c) 2018-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
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

#include "gtm_string.h"

#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrblk_create.h"
#include "mlk_shrhash_find_bucket.h"
#include "mlk_garbage_collect.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsbgtr.h"

#define MAX_TRIES 4

error_def(ERR_MLKHASHTABERR);

/**
 * Moves buckets starting at (hash % ctl->num_blkhash) to see if we can make room; returns the index of the new available bucket
 *   within range of goal bucket if such a bucket was found, or -1 if it could not be created
 *
 * @param [in] ctl LOCK control structure used for assertions and finding offset
 * @param [in] shrhash pointer to the shrhash data structure in the shared memory region
 * @param [in] hash to try and place
 * @param [in] region region to try performing garbage collection on if needed
 * @param [in] try_gc if true, a garbage collection is performed if we can't find space for the hash. Requires region to be defined
 * @returns the index of the slot available, or MLK_SHRHASH_FOUND_NO_BUCKET if no such slot could be found
 */
int mlk_shrhash_find_bucket(mlk_pvtctl_ptr_t pctl, uint4 hash)
{
	int			bi, fi, si, mi, loop_cnt;
	uint4			num_buckets;
	mlk_shrhash_map_t	usedmap;
	mlk_shrhash_ptr_t	free_bucket, search_bucket, move_bucket;
	mlk_shrblk_ptr_t	move_shrblk;
	mlk_shrhash_ptr_t	shrhash;
	sgmnt_addrs		*csa;

	shrhash = pctl->shrhash;
	num_buckets = pctl->shrhash_size;
	bi = hash % num_buckets;
	/* Search for free bucket.
	 * The hash table should never be full because it is larger than the max number of locks we can allocate.
	 */
	assert(pctl->ctl->blkcnt < num_buckets);
	for (fi = bi; 0 != shrhash[fi].shrblk_idx ; fi = (fi + 1) % num_buckets)
	{
		if ((fi + 1) % num_buckets == bi)
		{	/* Table full not possible because of prior assert about "pctl->ctl->blkcnt" greater than "num_buckets",
			 * i.e. we allocated more mlk_shrhash structures than mlk_shrsub structures in "mlk_shr_init". Hence
			 * the assert below.
			 */
			assert((fi + 1) % num_buckets != bi);
			return MLK_SHRHASH_FOUND_NO_BUCKET;
		}
	}
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
			for (mi = si, loop_cnt = 0 ; (loop_cnt < MLK_SHRHASH_NEIGHBORS) && (0 == (usedmap & 1U)) && (mi != fi) ;
					usedmap >>= 1, loop_cnt++)
			{
				assert(0 != usedmap);
				mi = (mi + 1) % num_buckets;
			}
			if ((mi != fi) && (loop_cnt != MLK_SHRHASH_NEIGHBORS))
				break;
		}
		if (si == fi)
		{	/* No movable buckets. Normally one needs to resize the hash table but since this hash
			 * table is in shared memory, it cannot be resized (needs shared memory size change which
			 * is not easily possible). Degenerate to linear search scheme for just this bucket.
			 */
			csa = &FILE_INFO(pctl->region)->s_addrs;
			/* Record this rare event in the file header */
			BG_TRACE_PRO_ANY(csa, lock_hash_bucket_full);
			fi = -(fi + 1);	/* negative value to indicate bucket full situation; "+ 1" done to handle 0 fi */
			assert(0 > fi);
			break;
		}
		/* Move the bucket from the mapped bucket to the free bucket */
		move_bucket = &shrhash[mi];
		free_bucket = &shrhash[fi];
		assert(0 != move_bucket->shrblk_idx);
		assert(0 == free_bucket->shrblk_idx);
		/* Check that moving neighbor was marked used and free neighbor was not */
		assert(IS_NEIGHBOR(search_bucket->usedmap, (num_buckets + mi - si) % num_buckets));
		assert(!IS_NEIGHBOR(search_bucket->usedmap, (num_buckets + fi - si) % num_buckets));
		/* We are moving a relative pointer, so convert to absolute and back */
		move_shrblk = MLK_SHRHASH_SHRBLK(*pctl, move_bucket);
		free_bucket->shrblk_idx = MLK_SHRBLK_IDX(*pctl, move_shrblk);
		assert(0 < free_bucket->shrblk_idx);
		assert(MLK_SHRHASH_NEIGHBORS > ((num_buckets + fi - (move_bucket->hash % num_buckets)) % num_buckets));
		free_bucket->hash = move_bucket->hash;
		move_bucket->shrblk_idx = 0;
		move_bucket->hash = 0;
		/* Clear bit for moved neighbor, set bit for target neighbor */
		CLEAR_NEIGHBOR(search_bucket->usedmap, (num_buckets + mi - si) % num_buckets);
		SET_NEIGHBOR(search_bucket->usedmap, (num_buckets + fi - si) % num_buckets);
		/* The moved neighbor is now free */
		fi = mi;
		assert(0 <= fi);
	}
	assert((MLK_SHRHASH_FOUND_NO_BUCKET != fi) && (-(MLK_SHRHASH_FOUND_NO_BUCKET + 1) != fi));
	return fi;
}
