/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "interlock.h"
#include "sleep_cnt.h"
#include "mupip_exit.h"
#include "copy.h"
#include "memcoherency.h"
#include "add_inter.h"
#include "shmpool.h"
#include "db_snapshot.h"

#define COMPUTE_BLOCK_OFFSET(blk, word, bit)	\
{						\
	word = blk / BLKS_PER_WORD;		\
	bit = blk % BLKS_PER_WORD;		\
}

boolean_t	ss_chk_shdw_bitmap(sgmnt_addrs *csa, snapshot_context_ptr_t lcl_ss_ctx, block_id blk)
{
	int			word, bit;
	unsigned int		*bitmap_addr;
	unsigned int		concerned_word;
	node_local_ptr_t	cnl;

	assert(NULL != lcl_ss_ctx);
	cnl = csa->nl;

	if (blk >= lcl_ss_ctx->total_blks)
		return FALSE;
	/* This function will be called by either GT.M while operating on phase 2 of the commit or by the snapshot initiator
	 * ss_release (the function that invalidates a snapshot) waits for the existing phase 2 commits to complete and
	 * the snapshot initiator will not call ss_release until it's done with it's operation. Hence the below assert is
	 * safe to be used
	 */
	assert((SNAPSHOTS_IN_PROG(csa) || SNAPSHOTS_IN_PROG(cnl)));
	bitmap_addr = (unsigned int *)(lcl_ss_ctx->bitmap_addr);	/* Point to the beginning of the shadow bitmap */
	/* It is possible that ss_chk_shdw_bitmap was called in the beginning of t_end or tp_tend where the snapshot context
	 * information cannot be relied upon always as we don't hold crit when we copy information from snapshot information in
	 * shared memory to the private snapshot context. So, the bitmap_addr value could be NULL in which case we don't want
	 * to proceed as dereferencing would cause SIG11. Assert accordingly. */
	if (NULL == bitmap_addr)
	{
		assert(!csa->wcs_pidcnt_incremented);
		return FALSE;
	}
	COMPUTE_BLOCK_OFFSET(blk, word, bit);
	assert(0 == ((long)bitmap_addr % 4));
	/* MUPIP INTEG when running with -ONLINE uses the following approach:
	 * 1. Checks if the shared memory has the bit turned ON for the block it's currently reading
	 * 	2. If TRUE, then read the block from the shadow temporary file
	 * 	3. If STEP 1 is FALSE, then read the block from the database file
	 *		4. Check if this bit in the shared memory for this block is still turned OFF (as it was in STEP 1)
	 *			5. If TRUE, proceed with the database block that was read in STEP 3
	 *			6. If STEP 5 is FALSE (means the bit is now turned ON), discard the database block and read
	 *			   from shadow temporary file
	 * If not memory barrier is issued below, then an out-of-sync MUPIP INTEG process (running on a processor whose cache
	 * is not in sync with other processes) might end up using the database block which is a post-update copy. So,
	 * issue a READ memory barrier here to receive the changes done by other processors.
	 */
	SHM_READ_MEMORY_BARRIER;
	concerned_word = (*(unsigned int *)(bitmap_addr + word));
	if (concerned_word & (1 << bit))
		return TRUE;
	else
		return FALSE;
}

void		ss_set_shdw_bitmap(sgmnt_addrs *csa, snapshot_context_ptr_t lcl_ss_ctx, block_id blk)
{
	unsigned int		*bitmap_addr;
	DEBUG_ONLY(unsigned int	prev_val;)
	int			word, bit;
	node_local_ptr_t	cnl;
	shm_snapshot_ptr_t	ss_shm_ptr;

	assert(NULL != lcl_ss_ctx);
	cnl = csa->nl;
	ss_shm_ptr = lcl_ss_ctx->ss_shm_ptr;
	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
	assert(blk < lcl_ss_ctx->total_blks);
	/* This function will be called by GT.M while operating on phase 2 of the commit. ss_release (the function that
	 * invalidates a snapshot) waits for the existing phase 2 commits to complete and Hence the below assert is safe to
	 * be used
	 */
	assert(SNAPSHOTS_IN_PROG(csa) && ss_shm_ptr->in_use);
	/* Before dereferencing the shared memory, verify if the shared memory that GT.M has attached to and the one that
	 * INTEG has created is actually in sync
	 */
	assert(lcl_ss_ctx->attach_shmid == cnl->ss_shmid);
	assert(ss_shm_ptr->ss_info.ss_shmid == lcl_ss_ctx->attach_shmid);
	bitmap_addr = (unsigned int *)(lcl_ss_ctx->bitmap_addr);	/* Point to the beginning of the shadow bitmap */
	COMPUTE_BLOCK_OFFSET(blk, word, bit);
	DEBUG_ONLY(prev_val = (*(unsigned int *)(bitmap_addr + word));)
	assert(0 == (prev_val & (1 << bit)));
	INTERLOCK_ADD((bitmap_addr + word), &ss_shm_ptr->bitmap_latch, (1 << bit));
	return;
}
