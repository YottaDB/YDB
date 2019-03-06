/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __MLK_OPS_H__
#define __MLK_OPS_H__

/* mlk_ops.h */

#include <sys/shm.h>

#include "interlock.h"
#include "do_shmat.h"

static inline void mlk_pvtctl_set_ctl(mlk_pvtctl_ptr_t pctl, mlk_ctldata_ptr_t ctl)
{
	pctl->ctl = ctl;
	if (NULL != pctl->ctl)
	{
		assert(pctl->ctl->blkbase);
		assert((MLK_CTL_BLKHASH_EXT == pctl->ctl->blkhash)
				|| (pctl->ctl != pctl->csa->mlkctl)
				|| (NULL == pctl->csa->mlkhash)
				|| (pctl->csa->mlkhash == (mlk_shrhash_ptr_t)R2A(pctl->ctl->blkhash)));
		pctl->shrblk = (mlk_shrblk_ptr_t)R2A(pctl->ctl->blkbase) - 1;
	} else
		assert(dba_cm == pctl->region->dyn.addr->acc_meth);
}

static inline void mlk_pvtctl_init(mlk_pvtctl_ptr_t pctl, struct gd_region_struct *reg)
{
	pctl->region = reg;
	pctl->csa = &FILE_INFO(pctl->region)->s_addrs;
	MLK_PVTCTL_SET_CTL(*pctl, pctl->csa->mlkctl);
	pctl->hash_fail_cnt = 0;
}

static inline void mlk_shrhash_insert(mlk_pvtctl_ptr_t pctl, int bucket_idx, int target_idx, uint4 shrblk_idx, uint4 hash)
{
	mlk_shrhash_ptr_t	bucket, target_bucket;
	uint4			num_buckets;

	num_buckets = pctl->shrhash_size;
	bucket = &pctl->shrhash[bucket_idx];
	target_bucket = &pctl->shrhash[target_idx];
	assert(!IS_NEIGHBOR(bucket->usedmap, (num_buckets + target_idx - bucket_idx) % num_buckets));
	assert(0 == target_bucket->shrblk_idx);
	target_bucket->shrblk_idx = shrblk_idx;
	assert(0 < target_bucket->shrblk_idx);
	assert(MLK_SHRHASH_NEIGHBORS > ((num_buckets + target_idx - bucket_idx) % num_buckets));
	target_bucket->hash = hash;
	/* Note the new neighbor of the original bucket */
	SET_NEIGHBOR(bucket->usedmap, (num_buckets + target_idx - bucket_idx) % num_buckets);
}

static inline void rel_lock_crit(mlk_pvtctl_ptr_t pctl, boolean_t was_crit)
{
	sgmnt_addrs	*csa;

	csa = pctl->csa;
	if (csa->lock_crit_with_db)
	{
		if (!was_crit)
			rel_crit(pctl->region);
	} else
		rel_latch(&csa->nl->lock_crit);
}

static inline void grab_lock_crit_intl(mlk_pvtctl_ptr_t pctl, boolean_t *ret_was_crit)
{
	GBLREF short	crash_count;
	sgmnt_addrs	*csa;

	csa = pctl->csa;
	if (csa->lock_crit_with_db)
	{
		if (csa->critical)
			crash_count = csa->critical->crashcnt;
		if (!(*ret_was_crit = csa->now_crit))		/* WARNING assignment */
			grab_crit(pctl->region);
	} else
	{	/* Return value of "grab_latch" does not need to be checked because we pass
		 * in GRAB_LATCH_INDEFINITE_WAIT as the timeout.
		 */
		grab_latch(&csa->nl->lock_crit, GRAB_LATCH_INDEFINITE_WAIT);
	}
}

static inline void grab_lock_crit_and_sync(mlk_pvtctl_ptr_t pctl, boolean_t *ret_was_crit)
{
	int	save_errno;

	assert(NULL != pctl->ctl);
	assert(NULL != pctl->csa);
	assert(NULL != pctl->shrblk);
	assert(pctl->ctl == pctl->csa->mlkctl);
	assert(pctl->shrblk == (mlk_shrblk_ptr_t)R2A(pctl->ctl->blkbase) - 1);
	do
	{
		grab_lock_crit_intl(pctl, ret_was_crit);
		if ((MLK_CTL_BLKHASH_EXT == pctl->ctl->blkhash)
			&& (pctl->ctl->hash_shmid != pctl->csa->mlkhash_shmid))
		{
			int	new_shmid;

			new_shmid = pctl->ctl->hash_shmid;
			rel_lock_crit(pctl, *ret_was_crit);
			if (pctl->csa->mlkhash_shmid != 0)
			{
				SHMDT(pctl->csa->mlkhash);
			}
			pctl->csa->mlkhash = do_shmat(new_shmid, NULL, 0);
			if (NULL != pctl->csa->mlkhash)
			{
				pctl->csa->mlkhash_shmid = new_shmid;
			} else
			{	/* We can encounter EINVAL due to a race with multiple shm switches, so just
				 * retry in that case.
				 */
				save_errno = errno;
				if (EINVAL != save_errno)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							LEN_AND_LIT("shmat"), CALLFROM, save_errno, 0);
			}
		} else
		{
			assert((MLK_CTL_BLKHASH_EXT == pctl->ctl->blkhash)
					|| (((char *)(pctl->ctl + 1) - (char *)&pctl->ctl->blkhash)
						== pctl->ctl->blkhash));
			if ((MLK_CTL_BLKHASH_EXT != pctl->ctl->blkhash) && (NULL == pctl->csa->mlkhash))
				pctl->csa->mlkhash = (mlk_shrhash_ptr_t)R2A(pctl->ctl->blkhash);
			break;
		}
	} while (TRUE);
	assert((MLK_CTL_BLKHASH_EXT == pctl->ctl->blkhash)
			|| (pctl->csa->mlkhash == (mlk_shrhash_ptr_t)R2A(pctl->ctl->blkhash)));
	pctl->shrhash = pctl->csa->mlkhash;
	pctl->shrhash_size = pctl->ctl->num_blkhash;
}

#endif
