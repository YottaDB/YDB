/****************************************************************
 *								*
 *	Copyright 2005, 2011 Fidelity Information Services, Inc	*
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
		VMS_ONLY(sblkh_p->image_count = -1);
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
			rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(reg), ERR_ERRCALL, 3, CALLFROM);
		}
		/* Can only verify queue *AFTER* get the lock */
		VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
		/* We have the lock and fast_lock_count is incremented to prevent deadlock on interrupt */
#ifdef VMS
		/* Checks to make sure one mode or the other isn't over using its resources. Only important
		   on VMS as reformat blocks do not exist on UNIX ports.
		*/
		if (SHMBLK_BACKUP == blktype)
		{
			/* See if backup has terminated itself for whatever reason */
			if (sbufh_p->backup_errno)
			{
				shmpool_unlock_hdr(reg);
				return (shmpool_blk_hdr_ptr_t)-1;
			}
			/* If we are in compatibility mode, backup is restricted to 50% of the buffers and must leave 1
			   reformat buffer available */
			if ((GDSV4 == csa->hdr->desired_db_format && (sbufh_p->total_blks / 2) < sbufh_p->backup_cnt)
			    || (sbufh_p->total_blks - 1) <= sbufh_p->backup_cnt)
			{	/* Too many backup blocks already in use for this type */
				if (MAX_BACKUP_FLUSH_TRY < attempts)
				{	/* We have tried too long .. backup errors out */
					sbufh_p->failed = process_id;
					sbufh_p->backup_errno = ERR_BCKUPBUFLUSH;
					csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
					shmpool_unlock_hdr(reg);
					return (shmpool_blk_hdr_ptr_t)-1;
				}
				limit_hit = TRUE;
			} else
				limit_hit = FALSE;
		} else if (SHMBLK_REFORMAT == blktype)
		{	/* If we are in compatibility mode and backup is active, reformat is restricted to 50% of the
			   buffers. Else  must always leave 1 backup buffer available */
			if (((GDSV4 == csa->hdr->desired_db_format && (sbufh_p->total_blks / 2) < sbufh_p->reformat_cnt)
			     && csa->backup_in_prog) || (sbufh_p->total_blks - 1) <= sbufh_p->reformat_cnt)
			{	/* Too many reformat blocks already in use */
				if (MAX_BACKUP_FLUSH_TRY < attempts)
				{	/* We have tried too long .. return non-buffer to signal do sync IO */
					shmpool_unlock_hdr(reg);
					return (shmpool_blk_hdr_ptr_t)-1;
				}
				limit_hit = TRUE;
			} else
				limit_hit = FALSE;
		} else
			GTMASSERT;
		if (!limit_hit)
#elif defined UNIX
		assert(SHMBLK_BACKUP == blktype);
		if (sbufh_p->backup_errno)
		{
			shmpool_unlock_hdr(reg);
			return (shmpool_blk_hdr_ptr_t)-1L;
		}
DEBUG_ONLY (
		if ((MAX_BACKUP_FLUSH_TRY / 2) == attempts)
			GET_C_STACK_FROM_SCRIPT("BCKUPBUFLUSH", process_id, sbufh_p->shmpool_crit_latch.u.parts.latch_pid, ONCE);
	   )
		if (MAX_BACKUP_FLUSH_TRY < attempts)
		{	/* We have tried too long .. backup errors out */
#ifdef DEBUG
			GET_C_STACK_FROM_SCRIPT("BCKUPBUFLUSH", process_id, sbufh_p->shmpool_crit_latch.u.parts.latch_pid, TWICE);
#else
			GET_C_STACK_FROM_SCRIPT("BCKUPBUFLUSH", process_id, sbufh_p->shmpool_crit_latch.u.parts.latch_pid, ONCE);
#endif
			sbufh_p->failed = process_id;
			sbufh_p->backup_errno = ERR_BCKUPBUFLUSH;
			csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			shmpool_unlock_hdr(reg);
			return (shmpool_blk_hdr_ptr_t)-1L;
		}
#endif
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
				VMS_ONLY(sblkh_p->image_count = image_count);
				assert(FALSE == sblkh_p->valid_data); /* cleared when blocok was freed .. should still be free */
				VMS_ONLY(if (SHMBLK_BACKUP == blktype))
				{
					insqt(&sblkh_p->sm_que, &sbufh_p->que_backup);
					++sbufh_p->backup_cnt;
					VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
#ifdef VMS
				} else
				{
					insqt(&sblkh_p->sm_que, &sbufh_p->que_reformat);
					++sbufh_p->reformat_cnt;
					VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_reformat);
#endif
				}
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
		VMS_ONLY(if (SHMBLK_BACKUP == blktype))
		{	/* Too many backup blocks already in use */
			BG_TRACE_PRO_ANY(csa, shmpool_alloc_bbflush);
			if (!backup_buffer_flush(reg))
			{	/* The lock was held by someone else, just do a micro sleep before cycling back
				   around to try again.
				*/
				wcs_sleep(LOCK_SLEEP);
			}
#ifdef VMS
			if ((sbufh_p->total_blks / 2) < sbufh_p->reformat_cnt)
				/* There are too many reformat blocks in use, make sure we recover
				   unused ones in a timely fashion.
				*/
				shmpool_harvest_reformatq(reg);
		} else
		{	/* Too many reformat blocks already in use */
			shmpool_harvest_reformatq(reg);
			if (attempts & 0x3)
				/* On all but every 4th pass, do a simple rel_quant */
				rel_quant();	/* Release processor to holder of lock (hopefully) */
			else
				/* On every 4th pass, we bide for awhile */
				wcs_sleep(LOCK_SLEEP);
			if ((sbufh_p->total_blks / 2) < sbufh_p->backup_cnt)
			{	/* There are too many backup blocks in use, make sure we recover
				   unused ones in a timely fashion.
				*/
				BG_TRACE_PRO_ANY(csa, shmpool_alloc_bbflush);
				backup_buffer_flush(reg);
			}
#endif
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
	assert(sbufh_p->shmpool_crit_latch.u.parts.latch_pid == process_id
	       VMS_ONLY(&& sbufh_p->shmpool_crit_latch.u.parts.latch_image_count == image_count));
	/* Verify queue only *AFTER* have the lock */
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
	assert(VMS_ONLY(SHMBLK_REFORMAT == sblkh_p->blktype ||) SHMBLK_BACKUP == sblkh_p->blktype);
	sblkh_p->holder_pid = 0;
	sblkh_p->valid_data = FALSE;
	VMS_ONLY(sblkh_p->image_count = -1);
	sblkh_p2 = (shmpool_blk_hdr_ptr_t)remqt((que_ent_ptr_t)((char_ptr_t)sblkh_p + sblkh_p->sm_que.fl));
	assert(sblkh_p2 == sblkh_p);	/* Check we dequ'd the element we want */
	DEBUG_ONLY(sblkh_p->sm_que.fl = 0; sblkh_p->sm_que.bl = 0);
	if (SHMBLK_BACKUP == sblkh_p->blktype)
	{
		--sbufh_p->backup_cnt;
		assert(0 <= sbufh_p->backup_cnt);
		VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
	}
#ifdef VMS
	else if (SHMBLK_REFORMAT == sblkh_p->blktype)
	{
		--sbufh_p->reformat_cnt;
		sblkh_p->use.rfrmt.cr_off = 0;		/* No longer queued to given CR */
		assert(0 <= sbufh_p->reformat_cnt);
		VERIFY_QUEUE(&(que_head_ptr_t)sbufh_p->que_reformat);
	}
#endif
	else
		GTMASSERT;
	sblkh_p->blktype = SHMBLK_FREE;
	++sbufh_p->free_cnt;
	insqt(&sblkh_p->sm_que, &sbufh_p->que_free);
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
	return;
}

#ifdef VMS
/* Routine to "harvest" reformat blocks that have complete IOs (VMS only). */
void shmpool_harvest_reformatq(gd_region *reg)
{
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p, next_sblkh_p;
	sgmnt_addrs		*csa;
	cache_rec_ptr_t		cr;
	unsigned int		iosb_cond;

	csa = &FILE_INFO(reg)->s_addrs;
	BG_TRACE_PRO_ANY(csa, shmpool_refmt_harvests);
	sbufh_p = csa->shmpool_buffer;
	if (FALSE == shmpool_lock_hdr(reg))
	{
		assert(FALSE);
		rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(reg), ERR_ERRCALL, 3, CALLFROM);
	}
	if (0 < sbufh_p->reformat_cnt)
	{	/* Only if there are some entries */
		assert(0 != sbufh_p->que_reformat.fl);
		for (sblkh_p = SBLKP_REL2ABS(&sbufh_p->que_reformat, fl);
		     sblkh_p != (shmpool_blk_hdr_ptr_t)&sbufh_p->que_reformat;
		     sblkh_p = next_sblkh_p)
		{	/* Loop through the queued reformat blocks */
			next_sblkh_p = SBLKP_REL2ABS(sblkh_p, fl);	/* Get next offset now in case remove entry */
			/* Check cache entry to see if it is for the same block and if the pending
			   IO is complete. */
			if (!sblkh_p->valid_data)
			{	/* Block is not in use yet */
				BG_TRACE_ANY(csa, refmt_hvst_blk_ignored);
				continue;
			}
			assert(SHMBLK_REFORMAT == sblkh_p->blktype);
			cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, sblkh_p->use.rfrmt.cr_off);
			/* Check that:

			   1) This block and the cr it points to are linked.
			   2) That the cycle is the same.

			   If we fail either of these, the link can be broken and this block released.
			*/
			if (0 == sblkh_p->use.rfrmt.cr_off
			    || (shmpool_blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->shmpool_blk_off) != sblkh_p
			    || cr->cycle != sblkh_p->use.rfrmt.cycle)
			{	/* Block no longer in (same) use .. release it */
				shmpool_blk_free(reg, sblkh_p);
				BG_TRACE_ANY(csa, refmt_hvst_blk_released_replaced);
			} else
			{	/* The link is intact. See if IO is done and recheck the cycle since we do not
				   have the region critical section here to prevent the cache record from being
				   modified while we are looking at it.
				*/
				SHM_READ_MEMORY_BARRIER;	/* Attempt to sync cr->* references */
				iosb_cond = cr->iosb.cond;
				if ((0 != iosb_cond && (WRT_STRT_PNDNG != iosb_cond))
				    || cr->cycle != sblkh_p->use.rfrmt.cycle)
				{	/* IO is complete or block otherwise reused. This means the IO ATTEMPT is
					   complete. This is not entirely the same check in wcs_wtfini() which cares
					   if the write was successful. We do not. If the write is retried, it will
					   be with a newly reformatted buffer, not this iteration of this one. Note
					   also that the missing part of this condition from wcs_wtfini() where an
					   is_proc_alive check is done is not done because whether the process is
					   alive or dead is wcs_wtfini's problem, not ours. When it clears up the
					   cache entry, we will clear up the associated reformat buffer, if
					   necessary.
					*/
					shmpool_blk_free(reg, sblkh_p);
					BG_TRACE_ANY(csa, refmt_hvst_blk_released_io_complete);
				} else
				{
					BG_TRACE_ANY(csa, refmt_hvst_blk_kept);
				}
			}

		}
	} else
		/* Shouldn't be any queue elements either */
		assert(0 == sbufh_p->que_reformat.fl);
	shmpool_unlock_hdr(reg);
}
#endif

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
	assert(process_id == sbufh_p->shmpool_crit_latch.u.parts.latch_pid
	       VMS_ONLY(&& sbufh_p->shmpool_crit_latch.u.parts.latch_image_count == image_count));
	/* This check only runs a maximum of once every N block allocations where N is the
	   number of blocks in this buffer pool.
	*/
	if (!force && sbufh_p->allocs_since_chk < sbufh_p->total_blks)
		return;		/* Too soon to run this check */

	BG_TRACE_PRO_ANY(csa, shmpool_recovery);
	sbufh_p->que_free.fl = sbufh_p->que_free.bl = 0;
	sbufh_p->que_backup.fl = sbufh_p->que_backup.bl = 0;
	VMS_ONLY(sbufh_p->que_reformat.fl = sbufh_p->que_reformat.bl = 0);
	sbufh_p->free_cnt = sbufh_p->backup_cnt = VMS_ONLY(sbufh_p->reformat_cnt = ) 0;
	sbufh_p->allocs_since_chk = 0;	/* Restart the counter */
	/* Rebuild the queues and counts according to:
	   1) What the block thinks it is (free, backup or reformat.
	   2) Verify that the block is in a valid state (needs pid set, process should exist)
	   3) Invalid state blocks are considered abandoned and put on the free queue.
	*/
	for (sblkh_p = (shmpool_blk_hdr_ptr_t)(sbufh_p + 1), blks = sbufh_p->total_blks;
	     blks > 0;
	     sblkh_p = (shmpool_blk_hdr_ptr_t)((char_ptr_t)sblkh_p + SIZEOF(shmpool_blk_hdr) + sbufh_p->blk_size), --blks)
	{	/* For each block not free, check if it is assigned to a process and if so if that process exists.
		   If not or if the block is already free, put on the free queue after a thorough cleaning.
		*/
		if (SHMBLK_FREE == sblkh_p->blktype
		    || 0 == sblkh_p->holder_pid || !is_proc_alive(sblkh_p->holder_pid, sblkh_p->image_count))
		{	/* Make sure block is clean (orphaned blocks might not be) and queue on free. */
			sblkh_p->holder_pid = 0;
			sblkh_p->valid_data = FALSE;
			sblkh_p->blktype = SHMBLK_FREE;
			VMS_ONLY(sblkh_p->image_count = -1);
			insqt(&sblkh_p->sm_que, &sbufh_p->que_free);
			++sbufh_p->free_cnt;
		} else if (SHMBLK_BACKUP == sblkh_p->blktype)
		{
			insqt(&sblkh_p->sm_que, &sbufh_p->que_backup);
			++sbufh_p->backup_cnt;
		} else
		{
#ifdef VMS
			if (SHMBLK_REFORMAT == sblkh_p->blktype)
			{
				insqt(&sblkh_p->sm_que, &sbufh_p->que_reformat);
				++sbufh_p->reformat_cnt;
			} else
#endif
				GTMASSERT;
		}
	}
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
	VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
	VMS_ONLY(VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_reformat));
	assert((sbufh_p->free_cnt + sbufh_p->backup_cnt VMS_ONLY(+ sbufh_p->reformat_cnt)) == sbufh_p->total_blks);

	sbufh_p->shmpool_blocked = FALSE;
	send_msg(VARLSTCNT(4) ERR_SHMPLRECOV, 2, REG_LEN_STR(reg));
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
			assert(latch->u.parts.latch_pid != process_id
			       VMS_ONLY(|| latch->u.parts.latch_image_count != image_count));

                        if (GET_SWAPLOCK(latch))
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
			/* assert(0 == (LOCK_TRIES % 4));  assures there are 3 rel_quants prior to first wcs_sleep()
			   but not needed as LOCK_TRIES is multiplied by 4 above.
			*/
			/* If near end of loop segment (LOCK_TRIES iters), see if target is dead and/or wake it up */
			if (RETRY_CASLATCH_CUTOFF == (retries % LOCK_TRIES))
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
	assert(latch->u.parts.latch_pid != process_id VMS_ONLY(|| latch->u.parts.latch_image_count != image_count));
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
	assert(process_id == latch->u.parts.latch_pid VMS_ONLY(&& image_count == latch->u.parts.latch_image_count));

	/* Quickly check if our counters are as we expect them to be. If not see if we need to run
	   our recovery procedure (shmpool_blk_abandoned_chk()).
	*/
	cntr_delta = sbufh_p->free_cnt + sbufh_p->backup_cnt VMS_ONLY(+ sbufh_p->reformat_cnt) - sbufh_p->total_blks;
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
