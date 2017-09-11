/****************************************************************
 *								*
 * Copyright (c) 2005-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*

  shmpool_buff.c is a collection of routines that collectively manage the 1MB shared buffer pool
  that is resident in all database shared memory segments. This pool of database blocksized buffers
  is used (currently) for a couple of purposes:

  1) When the DB output version is V4 on VMS, there needs to be a place where the reformatted buffer
     can live during the async I/O that VMS does. Those blocks are obtained from this pool.

  2) When backup needs to send blocks to the intermediate backup file, they are first written to these
     buffers and flushed at appropriate times.

  Rules for usage:

  - On VMS, if both backup and downgrade are in effect, neither service can use more than 50% of the
    available blocks. If a service is *not* being used, the other service may use all but one of the
    buffers.

*/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "relqop.h"
#include "copy.h"
#include "wcs_sleep.h"
#include "caller_id.h"
#include "rel_quant.h"
#include "sleep_cnt.h"
#include "interlock.h"
#include "is_proc_alive.h"
#include "mupipbckup.h"
#include "send_msg.h"
#include "performcaslatchcheck.h"
#include "gdsbgtr.h"
#include "lockconst.h"
#include "memcoherency.h"
#include "shmpool.h"
#include"gtm_c_stack_trace.h"

GBLREF	volatile int4		fast_lock_count;
GBLREF	pid_t			process_id;
GBLREF	uint4			image_count;
GBLREF	int			num_additional_processors;
GBLREF	node_local_ptr_t	locknl;
GBLREF	gd_region		*gv_cur_region;

error_def(ERR_BCKUPBUFLUSH);
error_def(ERR_DBCCERR);
error_def(ERR_ERRCALL);
error_def(ERR_SHMPLRECOV);

/* Initialize the shared memory buffer pool area */
void shmpool_buff_init(gd_region *reg)
{
	int			len, elem_len;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	sgmnt_addrs		*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;
	assert(0 == ((INTPTR_T)sbufh_p & (OS_PAGE_SIZE - 1)));	/* Locks and such optimally aligned */
	memset(sbufh_p, 0, SIZEOF(shmpool_buff_hdr));
	SET_LATCH_GLOBAL(&sbufh_p->shmpool_crit_latch, LOCK_AVAILABLE);
	len = SHMPOOL_BUFFER_SIZE - SIZEOF(shmpool_buff_hdr);	/* Length to build buffers with */
	elem_len = csa->hdr->blk_size + SIZEOF(shmpool_blk_hdr);
	assert((len & ~7) == len);				/* Verify 8 byte alignment */
	assert((elem_len & ~7) == elem_len);
	assert(len >= 2 * elem_len);				/* Need at least 1 rfmt and 1 bkup block */
	/* Place all buffers on free queue initially */
	for (sblkh_p = (shmpool_blk_hdr_ptr_t)(sbufh_p + 1);
	     len >= elem_len;
	     sblkh_p = (shmpool_blk_hdr_ptr_t)((char_ptr_t)sblkh_p + elem_len), len -= elem_len)
	{	/* For each buffer in the pool (however many there are) */
		sblkh_p->blktype = SHMBLK_FREE;
		sblkh_p->valid_data = FALSE;
		insqt(&sblkh_p->sm_que, (que_ent_ptr_t)&sbufh_p->que_free);
		++sbufh_p->free_cnt;
	}
	sbufh_p->total_blks = sbufh_p->free_cnt;
	sbufh_p->blk_size = csa->hdr->blk_size;
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
}


/* Allocate a block from the pool for our caller.

   Return: addr-of-block	on success
           -1			no block available
*/
shmpool_blk_hdr_ptr_t shmpool_blk_alloc(gd_region *reg, enum shmblk_type blktype)
{
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	sgmnt_addrs		*csa;
	int			attempts;
	boolean_t		limit_hit;

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;
	for (attempts = 1; ; ++attempts)	/* Start at 1 so we don't wcs_sleep() at bottom of loop on 1st iteration */
	{
		/* Try a bunch of times but release lock each time to give access to queues to processes that
		   are trying to free blocks they no longer need.
		*/
		if (FALSE == shmpool_lock_hdr(reg))
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(reg), ERR_ERRCALL, 3, CALLFROM);
		}
		/* Can only verify queue *AFTER* get the lock */
		VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
		/* We have the lock and fast_lock_count is incremented to prevent deadlock on interrupt */
		assert(SHMBLK_BACKUP == blktype);
		if (sbufh_p->backup_errno)
		{
			shmpool_unlock_hdr(reg);
			return (shmpool_blk_hdr_ptr_t)-1L;
		}
#		ifdef DEBUG
		if ((MAX_BACKUP_FLUSH_TRY / 2) == attempts)
			GET_C_STACK_FROM_SCRIPT("BCKUPBUFLUSH", process_id, sbufh_p->shmpool_crit_latch.u.parts.latch_pid, ONCE);
#		endif
		if (MAX_BACKUP_FLUSH_TRY < attempts)
		{	/* We have tried too long .. backup errors out */
#			ifdef DEBUG
			GET_C_STACK_FROM_SCRIPT("BCKUPBUFLUSH", process_id, sbufh_p->shmpool_crit_latch.u.parts.latch_pid, TWICE);
#			else
			GET_C_STACK_FROM_SCRIPT("BCKUPBUFLUSH", process_id, sbufh_p->shmpool_crit_latch.u.parts.latch_pid, ONCE);
#			endif
			sbufh_p->failed = process_id;
			sbufh_p->backup_errno = ERR_BCKUPBUFLUSH;
			csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			shmpool_unlock_hdr(reg);
			return (shmpool_blk_hdr_ptr_t)-1L;
		}
		{	/* We didn't hit the limit for this */
			/* Get the element (if available) */
			sblkh_p = (shmpool_blk_hdr_ptr_t)remqh(&sbufh_p->que_free);
			DEBUG_ONLY(if (sblkh_p) {sblkh_p->sm_que.fl = 0; sblkh_p->sm_que.bl = 0;} /* Mark elements as deq'd */);
			VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
			if (NULL != sblkh_p)
			{
				--sbufh_p->free_cnt;
				assert(SHMBLK_FREE == sblkh_p->blktype);
				/* Finish initializing block for return and queue to local queue to keep track of it */
				sblkh_p->blktype = blktype;
				sblkh_p->holder_pid = process_id;
				assert(FALSE == sblkh_p->valid_data); /* cleared when blocok was freed .. should still be free */
				insqt(&sblkh_p->sm_que, &sbufh_p->que_backup);
				++sbufh_p->backup_cnt;
				VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
				++sbufh_p->allocs_since_chk;
				shmpool_unlock_hdr(reg);
				return sblkh_p;
			}
		}
		/* Block buffer is not available -- call requisite reclaimation routine depending on blk type needed.
		  The availability of backup blocks is largely under our control (we can flush to get some) so sleeping
		  at this level won't free these up. However for reformat buffers, a short sleep to allow IO to (1) be
		  completed and (2) recognized by wcs_wtfini can take some time so some rel_quant()s and micro sleeps
		  are warranted.
		*/
		shmpool_unlock_hdr(reg);
		/* Too many backup blocks already in use */
		BG_TRACE_PRO_ANY(csa, shmpool_alloc_bbflush);
		if (!backup_buffer_flush(reg))
		{	/* The lock was held by someone else, just do a micro sleep before cycling back
			   around to try again.
			*/
			wcs_sleep(LOCK_SLEEP);
		}
	}
}

/* Release previously allocated block */
void shmpool_blk_free(gd_region *reg, shmpool_blk_hdr_ptr_t sblkh_p)
{
	shmpool_buff_hdr_ptr_t		sbufh_p;
	shmpool_blk_hdr_ptr_t 		sblkh_p2;
	sgmnt_addrs			*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;
	/* Make sure we have lock coming in */
	assert(sbufh_p->shmpool_crit_latch.u.parts.latch_pid == process_id);
	/* Verify queue only *AFTER* have the lock */
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
	assert(SHMBLK_BACKUP == sblkh_p->blktype);
	sblkh_p->holder_pid = 0;
	sblkh_p->valid_data = FALSE;
	sblkh_p2 = (shmpool_blk_hdr_ptr_t)remqt((que_ent_ptr_t)((char_ptr_t)sblkh_p + sblkh_p->sm_que.fl));
	assert(sblkh_p2 == sblkh_p);	/* Check we dequ'd the element we want */
	DEBUG_ONLY(sblkh_p->sm_que.fl = 0; sblkh_p->sm_que.bl = 0);
	if (SHMBLK_BACKUP == sblkh_p->blktype)
	{
		--sbufh_p->backup_cnt;
		assert(0 <= sbufh_p->backup_cnt);
		VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
	} else
		assertpro(FALSE);
	sblkh_p->blktype = SHMBLK_FREE;
	++sbufh_p->free_cnt;
	insqt(&sblkh_p->sm_que, &sbufh_p->que_free);
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
	return;
}

/* Check if we have any "lost" blocks by failed (and now defunct) processes.
   There are 3 reasons this routine can be called:

   (1) Called by shmpool_unlock_hdr() if the sum of the counts of the elements on the 3 queues differs from
       the known total number of blocks. There is a small variance allowed since a couple of missing blocks
       need not trigger a recovery but more than a handful triggers this code to reclaim the lost blocks and
       make the ocunters correct again.

   (2) If secshr_db_clnup() sets shmpool_blocked by releasing the lock of a shot/killed process during
       cleanup.

   (3) If after 15 (0xF) attempts to allocate a backup buffer after a buffer flush we still have no buffer
       to use then trigger this module in case the reason is lost blocks. It is possible for blocks to be
       lost and not trigger either (1) or (2) if, for example, a process is shot or errors out after allocating
       a backup block but before filling it in or allocating but before filling a reformat block.
*/
void shmpool_abandoned_blk_chk(gd_region *reg, boolean_t force)
{
	int			blks;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	sgmnt_addrs		*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;
	/* Note we must hold the shmpool latch *before* calling this routine */
	assert(process_id == sbufh_p->shmpool_crit_latch.u.parts.latch_pid);
	/* This check only runs a maximum of once every N block allocations where N is the
	   number of blocks in this buffer pool.
	*/
	if (!force && sbufh_p->allocs_since_chk < sbufh_p->total_blks)
		return;		/* Too soon to run this check */

	BG_TRACE_PRO_ANY(csa, shmpool_recovery);
	sbufh_p->que_free.fl = sbufh_p->que_free.bl = 0;
	sbufh_p->que_backup.fl = sbufh_p->que_backup.bl = 0;
	sbufh_p->free_cnt = sbufh_p->backup_cnt = 0;
	sbufh_p->allocs_since_chk = 0;	/* Restart the counter */
	/* Rebuild the queues and counts according to:
	   1) What the block thinks it is (free, backup or reformat)
	   2) Verify that the block is in a valid (valid_data) OR "in-flight" state (in use by an existing process)
	   3) Invalid state blocks are considered abandoned and put on the free queue.
	*/
	for (sblkh_p = (shmpool_blk_hdr_ptr_t)(sbufh_p + 1), blks = sbufh_p->total_blks;
	     blks > 0;
	     sblkh_p = (shmpool_blk_hdr_ptr_t)((char_ptr_t)sblkh_p + SIZEOF(shmpool_blk_hdr) + sbufh_p->blk_size), --blks)
	{	/* For each block not free, check if it is assigned to a process and if so if that process exists.
		   If not or if the block is already free, put on the free queue after a thorough cleaning.
		*/
		if ((SHMBLK_FREE == sblkh_p->blktype)
		    || (0 == sblkh_p->holder_pid)
		    || ((FALSE == sblkh_p->valid_data) && (!is_proc_alive(sblkh_p->holder_pid, sblkh_p->image_count))))
		{	/* Make sure block is clean (orphaned blocks might not be) and queue on free. */
			sblkh_p->holder_pid = 0;
			sblkh_p->valid_data = FALSE;
			sblkh_p->blktype = SHMBLK_FREE;
			insqt(&sblkh_p->sm_que, &sbufh_p->que_free);
			++sbufh_p->free_cnt;
		} else if (SHMBLK_BACKUP == sblkh_p->blktype)
		{
			insqt(&sblkh_p->sm_que, &sbufh_p->que_backup);
			++sbufh_p->backup_cnt;
		} else
			assertpro(FALSE);
	}
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
	assert((sbufh_p->free_cnt + sbufh_p->backup_cnt) == sbufh_p->total_blks);
	sbufh_p->shmpool_blocked = FALSE;
	send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_SHMPLRECOV, 2, REG_LEN_STR(reg));
}


/* Lock the shared memory buffer pool header. If cannot get the lock, return FALSE, else TRUE. */
boolean_t shmpool_lock_hdr(gd_region *reg)
{
	int			retries, spins, maxspins;
	sm_global_latch_ptr_t	latch;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	sgmnt_addrs		*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;
	latch = &sbufh_p->shmpool_crit_latch;
	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	/* Since LOCK_TRIES is approx 50 seconds, give us 4X that long since IO is involved */
	for (retries = (LOCK_TRIES * 4) - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{	/* We better not hold it if trying to get it */
			assert(latch->u.parts.latch_pid != process_id);
			if (GET_SWAPLOCK(latch))	/* seems this lock can be either short or long - two different forms? */
			{
				DEBUG_ONLY(locknl = csa->nl);
				LOCK_HIST("OBTN", latch, process_id, retries);
				DEBUG_ONLY(locknl = NULL);
				/* Note that fast_lock_count is kept incremented for the duration that we hold the lock
				   to prevent our dispatching an interrupt that could deadlock getting this lock
				*/
				/* If the buffer is marked as "blocked", first run a forced recovery before returning */
				if (sbufh_p->shmpool_blocked)
					shmpool_abandoned_blk_chk(reg, TRUE);
				return TRUE;
			}
		}
		if (retries & 0x3)
			/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			/* Check if we're due to check for lock abandonment check or holder wakeup */
			if (0 == (retries & (LOCK_CASLATCH_CHKINTVL - 1)))
				performCASLatchCheck(latch, TRUE);
		}
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return FALSE;
}

/* Lock the buffer header area like shmpool_lock_hdr() but if cannot get lock, do not wait for it */
boolean_t shmpool_lock_hdr_nowait(gd_region *reg)
{
	sm_global_latch_ptr_t	latch;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	sgmnt_addrs		*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;
	latch = &sbufh_p->shmpool_crit_latch;
	++fast_lock_count;			/* Disable wcs_stale for duration */
	/* We better not hold it if trying to get it */
	assert(latch->u.parts.latch_pid != process_id);
	if (GET_SWAPLOCK(latch))
	{
		DEBUG_ONLY(locknl = csa->nl);
		LOCK_HIST("OBTN", latch, process_id, -1);
		DEBUG_ONLY(locknl = NULL);
		/* Note that fast_lock_count is kept incremented for the duration that we hold the lock
		   to prevent our dispatching an interrupt that could deadlock getting this lock
		*/
		/* If the buffer is marked as "blocked", first run a forced recovery before returning */
		if (sbufh_p->shmpool_blocked)
			shmpool_abandoned_blk_chk(reg, TRUE);
		return TRUE;
	}
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	return FALSE;
}


/* Unlock the shared memory buffer pool header */
void shmpool_unlock_hdr(gd_region *reg)
{
	sm_global_latch_ptr_t	latch;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	sgmnt_addrs		*csa;
	int			cntr_delta;

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;
	latch = &sbufh_p->shmpool_crit_latch;
	assert(process_id == latch->u.parts.latch_pid);
	/* Quickly check if our counters are as we expect them to be. If not see if we need to run
	   our recovery procedure (shmpool_blk_abandoned_chk()).
	*/
	cntr_delta = sbufh_p->free_cnt + sbufh_p->backup_cnt - sbufh_p->total_blks;
	if (0 != cntr_delta)
		shmpool_abandoned_blk_chk(reg, FALSE);
	DEBUG_ONLY(locknl = csa->nl);
	LOCK_HIST("RLSE", latch, process_id, 0);
	RELEASE_SWAPLOCK(latch);
	DEBUG_ONLY(locknl = NULL);
	--fast_lock_count;
	assert(0 <= fast_lock_count);
}

/* If the shmpool lock is held by this process return true .. else false. */
boolean_t shmpool_lock_held_by_us(gd_region *reg)
{
	sgmnt_addrs		*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	return GLOBAL_LATCH_HELD_BY_US(&csa->shmpool_buffer->shmpool_crit_latch);
}
