/****************************************************************
 *								*
 * Copyright (c) 2018-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <sys/shm.h>

#include "gtm_ipc.h"

#include "mlk_shrhash_find_bucket.h"
#include "mlk_shrhash_resize.h"
#include "filestruct.h"
#include "iosp.h"
#include "ipcrmid.h"
#include "do_shmat.h"
#include "mlk_ops.h"
#include "mlk_rehash.h"

GBLREF	uint4	process_id;

error_def(ERR_MLKHASHRESIZE);
error_def(ERR_MLKHASHRESIZEFAIL);

#define SHRHASH_PAGE_SIZE		(gtm_uint8)(2 * 1024 * 1024)
#define NEW_SHRHASH_MEM(SHRHASH_SIZE)	ROUND_UP((SHRHASH_SIZE) * SIZEOF(mlk_shrhash) * 5 / 4, SHRHASH_PAGE_SIZE)

boolean_t mlk_shrhash_resize(mlk_pvtctl_ptr_t pctl)
{
	mlk_shrhash_ptr_t	shrhash_old, shrhash_new, old_bucket, new_bucket, free_bucket;
	int			obi, shmid_new, shmid_old, nbi, fi, save_errno, status;
	uint4			shrhash_size_old, shrhash_size_new, hash;
	size_t			shrhash_mem_new;
	mlk_pvtctl		pctl_new;

	assert(LOCK_CRIT_HELD(pctl->csa));
	shrhash_size_old = pctl->ctl->num_blkhash;
	shrhash_mem_new = NEW_SHRHASH_MEM(shrhash_size_old);
	shrhash_size_new = shrhash_mem_new / SIZEOF(mlk_shrhash);

	do
	{
		shmid_new = gtm_shmget(IPC_PRIVATE, shrhash_mem_new, RWDALL | IPC_CREAT, TRUE);
		assert(-1 != shmid_new);
		if (-1 == shmid_new)
		{
			send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(8)
					ERR_SYSCALL, 5, LEN_AND_LIT("shmget"), CALLFROM, errno, 0);
			return FALSE;
		}

		shrhash_new = do_shmat(shmid_new, NULL, 0);
		assert(NULL != shrhash_new);
		if (NULL == shrhash_new)
		{
			save_errno = errno;
			shm_rmid(shmid_new);		/* Ignore error return, as we are already in error state. */
			send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(8)
					ERR_SYSCALL, 5, LEN_AND_LIT("shmat"), CALLFROM, save_errno, 0);
			return FALSE;
		}

		shrhash_old = pctl->shrhash;
		pctl_new = *pctl;
		pctl_new.shrhash = shrhash_new;
		pctl_new.shrhash_size = shrhash_size_new;
		pctl_new.hash_fail_cnt = 0;

		for (obi=0 ; obi < shrhash_size_old ; obi++)
		{
			if (0 == shrhash_old[obi].shrblk_idx)
				continue;
			old_bucket = &shrhash_old[obi];
			hash = old_bucket->hash;
			nbi = hash % shrhash_size_new;
			new_bucket = &shrhash_new[nbi];
			fi = mlk_shrhash_find_bucket(&pctl_new, hash);
			assert(-1 != fi);
			if (-1 == fi)
			{	/* Hash insert failure */
				/* Try a larger hash table */
				status = SHMDT(shrhash_new);
				if (-1 == status)
					send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(8)
							ERR_SYSCALL, 5, LEN_AND_LIT("shmdt"), CALLFROM, errno, 0);
				status = shm_rmid(shmid_new);
				if (-1 == status)
					send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(8)
							ERR_SYSCALL, 5, LEN_AND_LIT("shm_rmid"), CALLFROM, errno, 0);
				send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(5)
						ERR_MLKHASHRESIZEFAIL, 3, shrhash_size_old, shrhash_size_new);
				if (shrhash_size_new > (pctl->ctl->max_blkcnt - pctl->ctl->blkcnt) * 2)
				{	/* We have more than twice as many hash buckets as we have active shrblks,
					 * indicating something pathological, so try rehashing instead.
					 */
					pctl->ctl->rehash_needed = TRUE;
					return FALSE;
				}
				shrhash_mem_new = NEW_SHRHASH_MEM(shrhash_size_new);
				shrhash_size_new = shrhash_mem_new / SIZEOF(mlk_shrhash);
				break;
			}
			/* We found one close enough, so store the new data there */
			mlk_shrhash_insert(&pctl_new, nbi, fi, old_bucket->shrblk_idx, hash);
		}
	} while(obi < shrhash_size_old);

	if (MLK_CTL_BLKHASH_EXT == pctl->ctl->blkhash)
	{	/* We already had a shared memory, so get rid of it. */
		shmid_old = pctl->csa->mlkhash_shmid;
		status = SHMDT(shrhash_old);
		if (-1 == status)
			send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(8)
					ERR_SYSCALL, 5, LEN_AND_LIT("shmdt"), CALLFROM, errno, 0);
		status = shm_rmid(shmid_old);
		if (-1 == status)
			send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(8)
					ERR_SYSCALL, 5, LEN_AND_LIT("shm_rmid"), CALLFROM, errno, 0);
	}

	*pctl = pctl_new;
	pctl->csa->mlkhash = shrhash_new;
	pctl->csa->mlkhash_shmid = pctl->ctl->hash_shmid = shmid_new;
	pctl->ctl->blkhash = MLK_CTL_BLKHASH_EXT;
	pctl->ctl->num_blkhash = shrhash_size_new;
	pctl->ctl->gc_needed = FALSE;
	pctl->ctl->resize_needed = FALSE;
	send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(5)
			ERR_MLKHASHRESIZE, 3, shrhash_size_old, shrhash_size_new, shmid_new);
	return TRUE;
}
