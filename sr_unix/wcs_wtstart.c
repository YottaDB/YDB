/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <signal.h>	/* for VSIG_ATOMIC_T type */
#include "util.h"
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
#include "wcs_flu.h"
#include "add_inter.h"
#include "wcs_recover.h"
#include "gtm_string.h"
#include "have_crit.h"
#include "gds_blk_downgrade.h"
#include "deferred_signal_handler.h"
#include "memcoherency.h"
#include "wbox_test_init.h"
#include "wcs_clean_dbsync.h"
#include "anticipatory_freeze.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "min_max.h"
#include "gtmimagename.h"

#define	REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, trace_cntr)	\
{									\
	n = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)ahead);		\
	if (INTERLOCK_FAIL == n)					\
	{								\
		assert(FALSE);						\
		SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);		\
		BG_TRACE_PRO_ANY(csa, trace_cntr);			\
		break;							\
	}								\
}

#define DBIOERR_LOGGING_PERIOD			100

GBLREF	boolean_t	*lseekIoInProgress_flags;	/* needed for the LSEEK* macros in gtmio.h */
GBLREF	uint4		process_id;
GBLREF	sm_uc_ptr_t	reformat_buffer;
GBLREF	int		reformat_buffer_len;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data	*cs_data;
#ifdef DEBUG
GBLREF	volatile int	reformat_buffer_in_use;
GBLREF	volatile int4	gtmMallocDepth;
#endif
/* In case of a disk-full situation, we want to print a message every 1 minute. We maintain two global variables to that effect.
 * dskspace_msg_counter and save_dskspace_msg_counter. If we encounter a disk-full situation and both those variables are different
 * we start a timer dskspace_msg_timer() that pops after a minute and increments one of the variables dskspace_msg_counter.
 * Since we want the first disk-full situation to also log a message, we initialise them to different values.
 */
static 	volatile uint4 		save_dskspace_msg_counter = 0;
GBLDEF	volatile uint4		dskspace_msg_counter = 1;	/* not static since used in dskspace_msg_timer.c */

error_def(ERR_DBFILERR);
error_def(ERR_DBIOERR);
error_def(ERR_ENOSPCQIODEFER);
error_def(ERR_GBLOFLOW);
error_def(ERR_JNLFSYNCERR);
error_def(ERR_JNLWRTDEFER);
error_def(ERR_JNLWRTNOWWRTR);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

int4	wcs_wtstart(gd_region *region, int4 writes)
{
	blk_hdr_ptr_t		bp, save_bp;
	boolean_t               need_jnl_sync, queue_empty, got_lock, bmp_status;
	cache_que_head_ptr_t	ahead;
	cache_state_rec_ptr_t	csr, csrfirst;
	int4                    err_status = 0, n, n1, n2, max_ent, max_writes, save_errno;
        size_t                  size ;
	jnl_buffer_ptr_t        jb;
        jnl_private_control     *jpc;
	node_local_ptr_t	cnl;
	off_t			blk_1_off, offset;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_ptr;
	uint4			saved_dsk_addr;
	unix_db_info		*udi;
	cache_rec_ptr_t		cr, cr_lo, cr_hi;
	static	int4		error_message_loop_count = 0;
	uint4			index;
	boolean_t		is_mm, was_crit;
	uint4			curr_wbox_seq_num;
	int			try_sleep, rc;
	gd_region		*sav_cur_region;
	sgmnt_addrs		*sav_cs_addrs;
	sgmnt_data		*sav_cs_data;
#	ifdef GTM_CRYPT
	char			*in, *out;
	int			in_len;
	int4			gtmcrypt_errno = 0;
	gd_segment		*seg;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (ANTICIPATORY_FREEZE_AVAILABLE)
		PUSH_GV_CUR_REGION(region, sav_cur_region, sav_cs_addrs, sav_cs_data)
	udi = FILE_INFO(region);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	is_mm = (dba_mm == csd->acc_meth);
	assert(is_mm || (dba_bg == csd->acc_meth));
	BG_TRACE_ANY(csa, wrt_calls);	/* Calls to wcs_wtstart */
	/* If *this* process is already in wtstart, we won't interrupt it do it again */
	if (csa->in_wtstart)
	{
		BG_TRACE_ANY(csa, wrt_busy);
		if (ANTICIPATORY_FREEZE_AVAILABLE)
			POP_GV_CUR_REGION(sav_cur_region, sav_cs_addrs, sav_cs_data)
		return err_status;			/* Already here, get out */
	}
	cnl = csa->nl;
	INCR_INTENT_WTSTART(cnl);	/* signal intent to enter wcs_wtstart */
	/* the above interlocked instruction does the appropriate write memory barrier to publish this change to the world */
	SHM_READ_MEMORY_BARRIER;	/* need to do this to ensure uptodate value of cnl->wc_blocked is read */
	if (cnl->wc_blocked)
	{
		DECR_INTENT_WTSTART(cnl);
		BG_TRACE_ANY(csa, wrt_blocked);
		if (ANTICIPATORY_FREEZE_AVAILABLE)
			POP_GV_CUR_REGION(sav_cur_region, sav_cs_addrs, sav_cs_data)
		return err_status;
	}
	csa->in_wtstart = TRUE;				/* Tell ourselves we're here and make the csa->in_wtstart (private copy) */
	INCR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);	/* and cnl->in_wtstart (shared copy) assignments as close as possible.   */
	SAVE_WTSTART_PID(cnl, process_id, index);
	assert(cnl->in_wtstart > 0 && csa->in_wtstart);

	max_ent = csd->n_bts;
	if (0 == (max_writes = writes))			/* If specified writes to do, use that.. */
		max_writes = csd->n_wrt_per_flu;	/* else, max writes is how many blocks there are */
	jpc = csa->jnl;
	assert(!JNL_ALLOWED(csd) || NULL != jpc);	/* if journaling is allowed, we better have non-null csa->jnl */
	if (JNL_ENABLED(csd) && (NULL != jpc) && (NOJNL != jpc->channel))
	{	/* Before flushing the database buffers, give journal flushing a nudge. Any failures in writing to the
		 * journal are not handled here since the main purpose of wcs_wtstart is to flush the database buffers
		 * (not journal buffers). The journal issue will be caught later (in jnl_flush or some other jnl routine)
		 * and appropriate errors, including triggering jnl_file_lost (if JNLCNTRL error) will be issued there.
		 */
		jnl_qio_start(jpc);
	}
	if (!is_mm)
	{
		ahead = &csa->acc_meth.bg.cache_state->cacheq_active;
		cr_lo = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
		cr_hi = cr_lo + csd->n_bts;
	} else
	{
		queue_empty = TRUE;
		n1 = 1; /* set to a non-zero value so dbsync timer canceling (if needed) can happen */
		goto writes_completed; /* to avoid unnecessary IF checks in the more common case (BG) */
	}
	assert(((sm_long_t)ahead & 7) == 0);
	queue_empty = FALSE;
	csa->wbuf_dqd++;			/* Tell rundown we have an orphaned block in case of interrupt */
	was_crit = csa->now_crit;
	for (n1 = n2 = 0, csrfirst = NULL; (n1 < max_ent) && (n2 < max_writes) && !cnl->wc_blocked; ++n1)
	{	/* If not-crit, avoid REMQHI by peeking at the active queue and if it is found to have a 0 fl link, assume
		 * there is nothing to flush and break out of the loop. This avoids unnecessary interlock usage (GTM-7635).
		 * If holding crit, we cannot safely avoid the REMQHI so interlock usage is avoided only in the no-crit case.
		 */
		if (!was_crit && (0 == ahead->fl))
			csr = NULL;
		else
		{
			csr = (cache_state_rec_ptr_t)REMQHI((que_head_ptr_t)ahead);
			if (INTERLOCK_FAIL == (INTPTR_T)csr)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtstart_lckfail1);
				break;
			}
		}
		if (NULL == csr)
			break;				/* the queue is empty */
		if (csr == csrfirst)
		{					/* completed a tour of the queue */
			queue_empty = FALSE;
			REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail2);
			break;
		}
		cr = (cache_rec_ptr_t)((sm_uc_ptr_t)csr - SIZEOF(cr->blkque));
		if (!is_mm)
		{
			assert(!CR_NOT_ALIGNED(cr, cr_lo) && !CR_NOT_IN_RANGE(cr, cr_lo, cr_hi));
			if (CR_BLKEMPTY == csr->blk)
			{	/* must be left by t_commit_cleanup - removing it from the queue and the following
				 * completes the cleanup
				 */
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
                                if ((csr->jnl_addr > jb->dskaddr)
				    || (need_jnl_sync && (NOJNL == jpc->channel
							  || (FALSE == (got_lock = GET_SWAPLOCK(&jb->fsync_in_prog_latch))))))
                                {
					if (need_jnl_sync)
						BG_TRACE_PRO_ANY(csa, n_jnl_fsync_tries);
					REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail3);
					if (NULL == csrfirst)
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
						GTM_JNL_FSYNC(csa, jpc->channel, rc);
						if (-1 == rc)
						{
							assert(FALSE);
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFSYNCERR, 2, JNL_LEN_STR(csd),
								 ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), errno);
							RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
							REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail3);
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
		{	/* sole owner */
			assert(WRITE_LATCH_VAL(csr) > LATCH_CLEAR);
			assert(0 == n);
			assert(0 != csr->dirty);
			/* We're going to write this block out now */
			save_errno = 0;
			if (!is_mm)
			{
				assert(FALSE == csr->data_invalid);	/* check that buffer has valid data */
				csr->epid = process_id;
				CR_BUFFER_CHECK1(region, csa, csd, cr, cr_lo, cr_hi);
				bp = (blk_hdr_ptr_t)(GDS_ANY_REL2ABS(csa, csr->buffaddr));
				VALIDATE_BM_BLK(csr->blk, bp, csa, region, bmp_status);	/* bmp_status holds bmp buffer's validity */
				assert(((blk_hdr_ptr_t)bp)->bver);	/* GDSV4 (0) version uses this field as a block length so
									   should always be > 0 */
				if (IS_GDS_BLK_DOWNGRADE_NEEDED(csr->ondsk_blkver))
				{	/* Need to downgrade/reformat this block back to a previous format. But, first defer timer
					 * or external interrupts from using the reformat buffer while we are modifying it.
					 */
					DEFER_INTERRUPTS(INTRPT_IN_REFORMAT_BUFFER_USE);
					assert(0 == reformat_buffer_in_use);
					DEBUG_ONLY(reformat_buffer_in_use++;)
					DEBUG_DYNGRD_ONLY(PRINTF("WCS_WTSTART: Block %d being dynamically downgraded on write\n", \
								 csr->blk));
					if (csd->blk_size > reformat_buffer_len)
					{	/* Buffer not big enough (or does not exist) .. get a new one releasing
						 * old if it exists
						 */
						assert(0 == gtmMallocDepth);	/* should not be in a nested free/malloc */
						if (reformat_buffer)
							free(reformat_buffer);	/* Different blksized databases in use
										   .. keep only largest one */
						reformat_buffer = malloc(csd->blk_size);
						reformat_buffer_len = csd->blk_size;
					}
					gds_blk_downgrade((v15_blk_hdr_ptr_t)reformat_buffer, (blk_hdr_ptr_t)bp);
					bp = (blk_hdr_ptr_t)reformat_buffer;
					size = (((v15_blk_hdr_ptr_t)bp)->bsiz + 1) & ~1;
				} else DEBUG_ONLY(if (GDSV6 == csr->ondsk_blkver))
					size = (bp->bsiz + 1) & ~1;
				DEBUG_ONLY(else GTMASSERT);
				if (csa->do_fullblockwrites)
					size = ROUND_UP(size, csa->fullblockwrite_len);
				assert(size <= csd->blk_size);
				offset = (csd->start_vbn - 1) * DISK_BLOCK_SIZE + (off_t)csr->blk * csd->blk_size;
				INCR_GVSTATS_COUNTER(csa, cnl, n_dsk_write, 1);
				save_bp = bp;
#				ifdef GTM_CRYPT
				if (csd->is_encrypted)
				{
					assert((unsigned char *)bp != reformat_buffer);
					DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, (sm_uc_ptr_t)bp);
					save_bp = (blk_hdr_ptr_t) GDS_ANY_ENCRYPTGLOBUF(bp, csa);
					DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, (sm_uc_ptr_t)save_bp);
					assert((bp->bsiz <= csd->blk_size) && (bp->bsiz >= SIZEOF(*bp)));
					in_len = MIN(csd->blk_size, bp->bsiz) - SIZEOF(*bp);
					if (BLK_NEEDS_ENCRYPTION(bp->levl, in_len))
					{
						ASSERT_ENCRYPTION_INITIALIZED;
						memcpy(save_bp, bp, SIZEOF(blk_hdr));
						in = (char *)(bp + 1);
						out = (char *)(save_bp + 1);
						GTMCRYPT_ENCRYPT(csa, csa->encr_key_handle, in, in_len, out, gtmcrypt_errno);
						save_errno = gtmcrypt_errno;
					} else
						memcpy(save_bp, bp, bp->bsiz);
				}
#				endif
				if (0 == save_errno)
				{	/* Due to csa->in_wtstart protection (at the beginning of this module), we are guaranteed
					 * that the write below won't be interrupted by another nested wcs_wtstart
					 */
					DB_LSEEKWRITE(csa, udi->fn, udi->fd, offset, save_bp, size, save_errno);
				}
				if ((blk_hdr_ptr_t)reformat_buffer == bp)
				{
					assert(INTRPT_OK_TO_INTERRUPT != intrpt_ok_state);
					DEBUG_ONLY(reformat_buffer_in_use--;)
					assert(0 == reformat_buffer_in_use);
					/* allow interrupts now that we are done using the reformat buffer */
					ENABLE_INTERRUPTS(INTRPT_IN_REFORMAT_BUFFER_USE);
				}
			}
			/* Trigger I/O error if white box test case is turned on */
			GTM_WHITE_BOX_TEST(WBTEST_WCS_WTSTART_IOERR, save_errno, ENOENT);
			if (0 != save_errno)
			{
				assert(ERR_ENOSPCQIODEFER != save_errno || !csa->now_crit);
				if (!is_mm)	/* before releasing update lock, clear epid as well in case of bg */
					csr->epid = 0;
				CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
				REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtstart_lckfail4);
				/* note: this will be automatically retried after csd->flush_time[0] msec, if this was called
				 * through a timer-pop, otherwise, error should be handled (including ignored) by the caller.
				 */
				if (ENOSPC == save_errno)
				{
					if (dskspace_msg_counter != save_dskspace_msg_counter)
					{	/* Report ENOSPC errors for first time and every minute after that. While this
						 * approach reduces the number of times ENOSPC is issued by every process, it still
						 * allows for flooding the syslog with ENOSPC messages if there are a lot of
						 * concurrent processes encountering ENOSPC errors. We should consider using a
						 * scheme like what is used limiting non-ENOSPC errors (see below) or MUTEXLCKALERT
						 * (see mutex.c)
						 * Also, may be DBFILERR should be replaced with DBIOERR for specificity.
						 */
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(region),
							 ERR_TEXT, 2, RTS_ERROR_TEXT("Error during flush write"), save_errno);
						if (!IS_GTM_IMAGE)
						{
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2,
									DB_LEN_STR(region), ERR_TEXT, 2,
									RTS_ERROR_TEXT("Error during flush write"), save_errno);
						}
						save_dskspace_msg_counter = dskspace_msg_counter;
						start_timer((TID)&dskspace_msg_timer, DSKSPACE_MSG_INTERVAL, dskspace_msg_timer, 0,
								NULL);
					}
				} else if(ERR_ENOSPCQIODEFER != save_errno)
				{
					cnl->wtstart_errcnt++;
					if (1 == (cnl->wtstart_errcnt % DBIOERR_LOGGING_PERIOD))
					{	/* Every 100th failed attempt, issue an operator log indicating an I/O error.
						 * wcs_wtstart is typically invoked during periodic flush timeout and since there
						 * cannot be more than 2 pending flush timers per region, number of concurrent
						 * processes issuing the below send_msg should be relatively small even if there
						 * are 1000s of processes.
						 */
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBIOERR, 4, REG_LEN_STR(region),
								DB_LEN_STR(region), save_errno);
					}
				}
				/* if (ERR_ENOSPCQIODEFER == save_errno): DB_LSEEKWRITE above encountered ENOSPC but couldn't
				 * trigger a freeze as it did not hold crit. It is okay to return as this is not a critical
				 * write. Eventually, some crit holding process will trigger a freeze and wait for space to be freed
				 * up.
				 */
				err_status = save_errno;
				break;
			}
			cnl->wtstart_errcnt = 0; /* Discard any previously noted I/O errors */
			++n2;
			BG_TRACE_ANY(csa, wrt_count);
			/* Detect whether queue has become empty. Defer action (calling wcs_clean_dbsync)
			 * to end of routine, since we still hold the lock on the cache-record
			 */
			queue_empty = !SUB_ENT_FROM_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
			INCR_CNT(&cnl->wc_in_free, &cnl->wc_var_lock);
			if (!is_mm)
			{
				csr->flushed_dirty_tn = csr->dirty;
				csr->epid = 0;
			}
			csr->dirty = 0;
			CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
			/* Note we are still under protection of wbuf_dqd lock at this point. Reason we keep
			 * it so long is so that all the counters are updated along with the queue being correct.
			 * The result of not doing this previously is that wcs_recover was NOT called when we
			 * got interrupted just prior to the counter adjustment leaving wcs_active_lvl out of
			 * sync with the actual count on the queue which caused an assert failure in wcs_flu. SE 11/2000
			 */
		}
	}
	csa->wbuf_dqd--;
writes_completed:
	DEBUG_ONLY(
		if (0 == n2)
			BG_TRACE_ANY(csa, wrt_noblks_wrtn);
		assert(cnl->in_wtstart > 0 && csa->in_wtstart);
	)
	if (csa->dbsync_timer && n1)
	{	/* If we already have a dbsync timer active AND we found at least one dirty cache record in the active queue
		 * now, this means there has not been enough time period of idleness since the last update and so there is
		 * no purpose to the existing timer. A new one would anyways be started whenever the last dirty cache
		 * record in the current active queue is flushed. Cancel the previous one.
		 */
		CANCEL_DBSYNC_TIMER(csa);
	}
	DECR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);
	CLEAR_WTSTART_PID(cnl, index);
	/* Defer interrupts to protect the decrement of cnl->intent_wtstart and potential addition of a new dbsync timer */
	DEFER_INTERRUPTS(INTRPT_IN_WCS_WTSTART);
	csa->in_wtstart = FALSE;			/* This process can write again */
	DECR_INTENT_WTSTART(cnl);
	if (queue_empty)			/* Active queue has become empty. */
		wcs_clean_dbsync_timer(csa);	/* Start a timer to flush-filehdr (and write epoch if before-imaging) */
	ENABLE_INTERRUPTS(INTRPT_IN_WCS_WTSTART);
#	ifdef GTM_CRYPT
	if (0 != gtmcrypt_errno)
	{	/* Now that we have done all cleanup (reinserted the cache-record that failed the write and cleared cnl->in_wtstart
		 * and cnl->intent_wtstart, go ahead and issue the error.
		 */
		seg = region->dyn.addr;
		GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
	}
#	endif
	if (ANTICIPATORY_FREEZE_AVAILABLE)
		POP_GV_CUR_REGION(sav_cur_region, sav_cs_addrs, sav_cs_data)
	return err_status;
}
