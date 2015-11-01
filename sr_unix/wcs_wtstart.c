/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "gtm_stdio.h"

#include "aswp.h"
#include "copy.h"
#include "dskspace_msg_timer.h"		/* needed for dskspace_msg_timer() declaration and DSKSPACE_MSG_INTERVAL macro value */
#include "error.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "iosp.h"	/* required for SS_NORMAL for use with msyncs */
#include "interlock.h"
#include "io.h"
#include "gdsbgtr.h"
#include "gtmio.h"
#include "relqueopi.h"
#include "gt_timer.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "tp_grab_crit.h"
#include "wcs_flu.h"
#include "add_inter.h"
#include "wcs_recover.h"
#include "gtm_string.h"

GBLREF	boolean_t		*lseekIoInProgress_flags;	/* needed for the LSEEK* macros in gtmio.h */
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	uint4			process_id;

/* In case of a disk-full situation, we want to print a message every 1 minute. We maintain two global variables to that effect.
 * dskspace_msg_counter and save_dskspace_msg_counter. If we encounter a disk-full situation and both those variables are different
 * we start a timer dskspace_msg_timer() that pops after a minute and increments one of the variables dskspace_msg_counter.
 * Since we want the first disk-full situation to also log a message, we initialise them to different values.
 */
static 	volatile uint4 		save_dskspace_msg_counter = 0;
GBLDEF	volatile uint4		dskspace_msg_counter = 1;	/* not static since used in dskspace_msg_timer.c */

void	wcs_sync_epoch(TID tid, int4 hd_len, jnl_private_control **jpcptr);

int4	wcs_wtstart(gd_region *region, int4 writes)
{
	blk_hdr_ptr_t		bp;
	boolean_t               need_jnl_sync, queue_empty, got_lock, bmp_status;
	cache_que_head_ptr_t	ahead;		/* serves dual purpose since cache_que_head = mmblk_que_head */
	cache_state_rec_ptr_t	csr, csrfirst;	/* serves dual purpose for MM and BG */
						/* since mmblk_state_rec is equal to the top of cache_state_rec */
	int4                    err_status = 0, n, n1, n2, max_ent, max_writes, size, save_errno;
	jnl_buffer_ptr_t        jb;
        jnl_private_control     *jpc;
	node_local_ptr_t	cnl;
	off_t			blk_1_off, offset;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_ptr;
	uint4			saved_dsk_addr;
	unix_db_info		*udi;
	static	int4		error_message_loop_count = 0;

	error_def(ERR_ERRCALL);
	error_def(ERR_DBCCERR);
	error_def(ERR_DBFILERR);
	error_def(ERR_JNLFSYNCERR);
	error_def(ERR_TEXT);
	error_def(ERR_JNLWRTNOWWRTR);
	error_def(ERR_JNLWRTDEFER);

	udi = FILE_INFO(region);
	csa = &udi->s_addrs;
	csd = csa->hdr;

	/* you don't enter this routine if this has been compiled with #define UNTARGETED_MSYNC and it is MM mode */
#if defined(UNTARGETED_MSYNC)
	assert(dba_mm != csd->acc_meth);
#endif

	BG_TRACE_ANY(csa, wrt_calls);	/* Calls to wcs_wtstart */
	if (csd->wc_blocked)
	{
		BG_TRACE_ANY(csa, wrt_blocked);
		return err_status;
	}
	/* If *this* process is already in wtstart, we won't interrupt it do it again */
	if (csa->in_wtstart)
	{
		BG_TRACE_ANY(csa, wrt_busy);
		return err_status;			/* Already here, get out */
	}
	csa->in_wtstart = TRUE;				/* Tell ourselves we're here */

	max_ent = csd->n_bts;
	if (0 == (max_writes = writes))			/* If specified writes to do, use that.. */
		max_writes = csd->n_wrt_per_flu;	/* else, max writes is how many blocks there are */
	cnl = csa->nl;
	jpc = csa->jnl;
	assert(csa == cs_addrs);
	assert(!JNL_ENABLED(csd) || NULL != jpc);

	if (dba_bg == csd->acc_meth)
	{
		if (JNL_ENABLED(csd)  &&  NULL != jpc && (NOJNL != jpc->channel))
		{
			if (SS_NORMAL != (err_status = jnl_qio_start(jpc)))
			{
				if (ERR_JNLWRTNOWWRTR != err_status && ERR_JNLWRTDEFER != err_status)
				{
					jpc->jnl_buff->blocked = 0;
					jnl_file_lost(jpc, err_status);
				}
			}
		}
		ahead = &csa->acc_meth.bg.cache_state->cacheq_active;
	} else
	{
		ahead = &csa->acc_meth.mm.mmblk_state->mmblkq_active;
		if (cnl->mm_extender_pid == process_id)
			max_writes = max_ent;		/* allow file extender or rundown to write everything out */
	}
	assert(((sm_long_t)ahead & 7) == 0);
	queue_empty = FALSE;
	INCR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);

	for (n1 = n2 = 0, csrfirst = NULL;  n1 < max_ent  &&  n2 < max_writes  &&  !csd->wc_blocked ;  ++n1)
	{
		assert(FALSE == csa->wbuf_dqd);
		csa->wbuf_dqd = TRUE;			/* Tell rundown we have an orphaned block in case of interrupt */
		csr = (cache_state_rec_ptr_t)REMQHI((que_head_ptr_t)ahead);
		if (CR_NOTVALID == (sm_long_t)csr)
		{					/* shouldn't be on the queue unless it's valid */
			csa->wbuf_dqd = FALSE;
			assert(FALSE);
			continue;			/* in production, it's off the queue, so just go on */
		}
		if (NULL == csr)
		{
			csa->wbuf_dqd = FALSE;
			break;				/* the queue is empty */
		}
		if (csr == csrfirst)
		{					/* completed a tour of the queue */
			queue_empty = FALSE;
			n = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)ahead);
			if (n == INTERLOCK_FAIL)
				rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, DB_LEN_STR(region), ERR_ERRCALL, 3, CALLFROM);
			csa->wbuf_dqd = FALSE;
			break;
		}
		if (dba_bg == csd->acc_meth)
		{
			if (CR_BLKEMPTY == csr->blk)
			{	/* must be left by t_commit_cleanup - removing it from the queue and the following
				   completes the cleanup */
				csa->wbuf_dqd = FALSE;
				assert(0 != csr->dirty);
				assert(csr->data_invalid);

				csr->data_invalid = FALSE;
				csr->dirty = 0;
				INCR_CNT(&cnl->wc_in_free, &cnl->wc_var_lock);
				queue_empty = !SUB_ENT_FROM_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
				continue;
			}
			/* If journaling, write only if the journal file is up to date and no jnl-switches occurred */
			if (JNL_ENABLED(csd))
                        {
                                jb = jpc->jnl_buff;
                                need_jnl_sync = (csr->jnl_addr > jb->fsync_dskaddr);
                                assert(!need_jnl_sync || jpc->channel != NOJNL || cnl->wcsflu_pid != process_id);
				got_lock = FALSE;
                                if ((csr->jnl_addr > jb->dskaddr) ||
				    (need_jnl_sync && (NOJNL == jpc->channel ||
						       (FALSE == (got_lock = GET_SWAPLOCK(&jb->fsync_in_prog_latch)))
						       )
				     )
				    )
                                {
                                        if (need_jnl_sync)
                                                BG_TRACE_PRO_ANY(csa, n_jnl_fsync_tries);
					n = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)ahead);
                                        if (n == INTERLOCK_FAIL)
                                                rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, DB_LEN_STR(region),
											ERR_ERRCALL, 3, CALLFROM);
                                        csa->wbuf_dqd = FALSE;
                                        if (csrfirst == NULL)
                                                csrfirst = csr;
                                        continue;
                                } else if (got_lock)
                                {
                                        saved_dsk_addr = jb->dskaddr;
					if (jpc->sync_io)
					{
						/* We need to maintain the fsync control fields irrespective of the type of IO,
						 * because we might switch between these at any time.
						 */
						jb->fsync_dskaddr = saved_dsk_addr;
					} else
					{
						if (-1 == fsync(jpc->channel))
						{
							assert(FALSE);
							send_msg(VARLSTCNT(9) ERR_JNLFSYNCERR, 2, JNL_LEN_STR(region),
								ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), errno);
							RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
							csa->wbuf_dqd = FALSE;
							if (NULL == csrfirst)
								csrfirst = csr;
							continue;
						} else
						{
							jb->fsync_dskaddr = saved_dsk_addr;
							BG_TRACE_PRO_ANY(csa, n_jnl_fsyncs);
						}
					}
                                        RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
                                }
                        }
		}
		LOCK_BUFF_FOR_WRITE(csr, n, &cnl->db_latch);
		assert(WRITE_LATCH_VAL(csr) >= LATCH_CLEAR);
		assert(WRITE_LATCH_VAL(csr) <= LATCH_CONFLICT);
		if (OWN_BUFF(n))
		{			/* sole owner */
			assert(0 == n);
			assert(0 != csr->dirty);
			/* We're going to write this block out now */
			if (dba_bg == csd->acc_meth)
			{
				csr->epid = process_id;
				bp = (blk_hdr_ptr_t)(GDS_ANY_REL2ABS(csa, csr->buffaddr));
				VALIDATE_BM_BLK(csr->blk, bp, csa, region, bmp_status);	/* bmp_status holds bmp buffer's validity */
#ifdef FULLBLOCKWRITES
				size = csd->blk_size;
#else
				size = bp->bsiz;
				assert(size <= csd->blk_size);
#endif
				offset = (csd->start_vbn - 1) * DISK_BLOCK_SIZE + (off_t)csr->blk * csd->blk_size;
				INCR_DB_CSH_COUNTER(csa, n_dsk_writes, 1);
				/* Do db write without timer protect (not needed since wtstart not reenterable in one task) */
				LSEEKWRITE(udi->fd, offset, bp, size, save_errno);
			} else
			{
#if defined(TARGETED_MSYNC)
			        bp = (blk_hdr_ptr_t)(csa->db_addrs[0] + (sm_off_t)csr->blk * MSYNC_ADDR_INCS);
				if ((sm_uc_ptr_t)bp > csa->db_addrs[1])
				{
					wcs_recover(region);
					csd = csa->hdr;
					bp = (blk_hdr_ptr_t)(csa->db_addrs[0] +
						(sm_off_t)csr->blk * MSYNC_ADDR_INCS);
					assert((sm_uc_ptr_t)bp < csa->db_addrs[1]);
				}
				size = MSYNC_ADDR_INCS;
				save_errno = 0;			/* Assume all will work well */
				if (-1 == msync((caddr_t)bp, MSYNC_ADDR_INCS, MS_ASYNC))
				        save_errno = errno;
#else
				bp = (blk_hdr_ptr_t)(csa->acc_meth.mm.base_addr + (sm_off_t)csr->blk * csd->blk_size);
				if ((sm_uc_ptr_t)bp > csa->db_addrs[1])
				{
					wcs_recover(region);
					csd = csa->hdr;
					bp = (blk_hdr_ptr_t)(csa->acc_meth.mm.base_addr +
						(sm_off_t)csr->blk * csd->blk_size);
					assert((sm_uc_ptr_t)bp < csa->db_addrs[1]);
				}
#ifdef FULLBLOCKWRITES
				size = csd->blk_size;
#else
				size = bp->bsiz;
				assert(size <= csd->blk_size);
#endif
				offset = (off_t)((sm_uc_ptr_t)bp - (sm_uc_ptr_t)csd);
				INCR_DB_CSH_COUNTER(csa, n_dsk_writes, 1);
				/* Do db write without timer protect (not needed since wtstart not reenterable in one task) */
				LSEEKWRITE(udi->fd, offset, bp, size, save_errno);
#endif
			}
			if (0 != save_errno)
			{
				CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
                                n = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)ahead);
				if (n == INTERLOCK_FAIL)
					rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, DB_LEN_STR(region), ERR_ERRCALL, 3, CALLFROM);
				csa->wbuf_dqd = FALSE;
				/* note: this will be automatically retried after csd->flush_time[0] msec, if this was called
				 * through a timer-pop, otherwise, error should be handled (including ignored) by the caller.
				 */
				if (dskspace_msg_counter != save_dskspace_msg_counter)
				{	/* first time and every minute */
					send_msg(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(region),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error during flush write"), save_errno);
					gtm_putmsg(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(region),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error during flush write"), save_errno);
					save_dskspace_msg_counter = dskspace_msg_counter;
					start_timer((TID)&dskspace_msg_timer, DSKSPACE_MSG_INTERVAL, dskspace_msg_timer, 0, NULL);
				}
				assert(ENOSPC == save_errno); 	/* Out-of-space should not be considered severe error */
				err_status = save_errno;
				break;
			}
			++n2;
			BG_TRACE_ANY(csa, wrt_count);
			/* Detect whether queue has become empty. Defer action (calling wcs_sync_epoch)
			 * to end of routine, since we still hold the lock on the cache-record */
			queue_empty = !SUB_ENT_FROM_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
			INCR_CNT(&cnl->wc_in_free, &cnl->wc_var_lock);
			if (dba_bg == csd->acc_meth)
			{
				csr->flushed_dirty_tn = csr->dirty;
				csr->epid = 0;
			}
			csr->dirty = 0;
			CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
			/* Note we are still under protection of wbuf_dqd lock at this point. Reason we keep
			   it so long is so that all the counters are updated along with the queue being correct.
			   The result of not doing this previously is that wcs_recover was NOT called when we
			   got interrupted just prior to the counter adjustment leaving wcs_active_lvl out of
			   sync with the actual count on the queue which caused an assert failure in wcs_flu. SE 11/2000
			*/
		}
		csa->wbuf_dqd = FALSE;
	}
	DEBUG_ONLY(
		if (0 == n2)
			BG_TRACE_ANY(csa, wrt_noblks_wrtn);
	)
	assert(cnl->in_wtstart > 0 && csa->in_wtstart);
	DECR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);
	csa->in_wtstart = FALSE;			/* This process can write again */
	if (queue_empty)			/* Active queue has become empty. */
		wcs_clean_dbsync_timer(csa);	/* Start a timer to flush-filehdr (and write epoch if before-imaging) */
	return err_status;
}
