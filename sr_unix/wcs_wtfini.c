/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "interlock.h"
#include "relqueopi.h"
#include "gdsbgtr.h"
#include "aio_shim.h"
#include "gtmio.h"
#include "is_proc_alive.h"
#include "anticipatory_freeze.h"
#include "add_inter.h"
#include "gtm_multi_proc.h"	/* for "multi_proc_in_use" GBLREF */
#include "wcs_wt.h"
#include "hashtab_int4.h"
#include "performcaslatchcheck.h"
#include "wcs_sleep.h"
#include "caller_id.h"
#include "rel_quant.h"
#include "sleep_cnt.h"
#include "gtm_c_stack_trace.h"
#include "copy.h"
#include "relqop.h"

#define	REQUEUE_TO_FREE		0
#define	REQUEUE_TO_WIP		1
#define	REQUEUE_TO_ACTIVE	2

#define	WTFINI_PID_ALIVE_HT_INITIAL_SIZE	4
#ifdef DEBUG
/* Every N successful writes, simulate an error in one of the writes. This is to exercise "wcs_wt_restart" code.
 * We do not want the frequency to be too low resulting in lot of rewrites. Neither do we want it too high as
 * that would mean no coverage of "wcs_wt_restart". Hence the particular value chosen below.
 */
#define	FAKE_WTERROR_FREQUENCY	256
#endif

STATICDEF hash_table_int4	wtfini_pid_ht;
STATICDEF boolean_t		wtfini_pid_ht_reinitialized = TRUE;
#ifdef DEBUG
STATICDEF int			dbg_skip_wcs_wt_restart;
#endif

GBLREF	volatile	int4	fast_lock_count;
GBLREF	int4		wtfini_in_prog;
GBLREF	uint4		process_id;
GBLREF	gd_region	*gv_cur_region;

error_def(ERR_DBCCERR);

/* Note: In case caller has read_only access to db, wcs_wtfini cleans up finished qios but does not initiate new ones.
 * Returns: 0 in case of SUCCESS. non-zero (== errno) in case of FAILURE.
 */
int	wcs_wtfini(gd_region *reg, boolean_t do_is_proc_alive_check, cache_rec_ptr_t cr2flush)
{
	boolean_t		new_pid, pid_alive;
	cache_que_head_ptr_t	ahead, whead;
	cache_rec_ptr_t		cr;
#	ifdef DEBUG
	cache_rec_ptr_t		cr_lo, cr_hi;
#	endif
	cache_state_rec_ptr_t	csr, start_csr;
	int			requeue, ret_value;
	int			restart_errno, status;
	int			aio_errno, aio_retval;
	int4			n;
	node_local_ptr_t	cnl;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	unix_db_info		*udi;
	unsigned int		n_bts;
	int			lcnt;
	uint4			epid;
	ht_ent_int4		*tabent;
	que_ent_ptr_t		next, prev, qent;
	void_ptr_t		retcsrptr;

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(csd->asyncio);	/* caller should have ensured this */
	cnl = csa->nl;
	assert(dba_bg == csd->acc_meth);
	assert(csa->now_crit);	/* Or else "bg_update_phase1" (which holds crit) would get confused if a concurrent non-crit
				 * process is running "wcs_wtfini" at the same time since it makes assumptions on the state of
				 * OLDER and NEWER twins based on cr->dirty etc. all of which could be concurrently changing
				 * in case "wcs_wtfini" can be invoked outside of crit.
				 */
	BG_TRACE_PRO_ANY(csa, wcs_wtfini_invoked);
	wtfini_in_prog++;
	cnl->wtfini_in_prog = process_id;
	ahead = &csa->acc_meth.bg.cache_state->cacheq_active;
	whead = &csa->acc_meth.bg.cache_state->cacheq_wip;
	n_bts = csd->n_bts;
#	ifdef DEBUG
	cr_lo = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	cr_hi = cr_lo + n_bts;
#	endif
	ret_value = 0;
	CHECK_ERROR_IN_WORKER_THREAD(reg, udi);
	if (do_is_proc_alive_check)
	{	/* Check if process is alive. But avoid calling "is_proc_alive" more than once per pid
		 * (system call) by maintaining a hash table of pids that we have already called for.
		 * Use hashtable if pid is found. If not use "is_proc_alive".
		 * This way if there are thousands of cache-records in the WIP queue corresponding to dead
		 * pids, we will not do thousands of "kill" system calls while holding crit in "wcs_wtfini".
		 */
		/* Reinitialize hash table if not cleared from previous invocation of "wcs_wtfini" */
		if (!wtfini_pid_ht_reinitialized)
		{
			reinitialize_hashtab_int4(&wtfini_pid_ht);
			wtfini_pid_ht_reinitialized = TRUE;
		} else if (0 == wtfini_pid_ht.size)
			init_hashtab_int4(&wtfini_pid_ht, WTFINI_PID_ALIVE_HT_INITIAL_SIZE, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
	}
	for (lcnt = cr2flush ? 0 : n_bts, start_csr = NULL; lcnt >= 0; lcnt--)
	{
		/* we will be attempting to take a cr off the wip queue for processing. We do not need the wbuf_dqd protection
		 * used by wcs_get_space() and wcs_wtstart() since wcs_wtfini has crit and will have wc_blocked set anyway
		 * if we get killed.
		 */
		if (cr2flush)
		{	/* asked to flush a specific cr: */
			/* should be dirty and have had a write issued, i.e., in the wip queue */
			csr = NULL; /* assume it's none until we find it */
			if (cr2flush->dirty && cr2flush->epid)
			{
				/* the if check implies cr2flush is out of the active queue at this point and is either already in
				 * the wip queue or about to be inserted into the wip queue. cr2flush->state_que.fl being non-zero
				 * (checked after the grab_latch below) would imply it is in the wip queue.
				 */
				++fast_lock_count; /* Disable wcs_stale for duration */
				if (grab_latch(&whead->latch, WT_LATCH_TIMEOUT_SEC))
                       		{
					cr = cr2flush;
					csr = (cache_state_rec_ptr_t)((sm_uc_ptr_t)cr + SIZEOF(cr->blkque));
					/* now that we have the wip queue header lock ensure cr2flush is still on the wip queue */
					if (cr2flush->dirty && cr2flush->epid && cr2flush->state_que.fl)
					{	/* the entry is in the wip queue */
						assert(cr2flush->dirty);
						assert(cr2flush->epid);
						assert(csr);
						assert(csr->state_que.bl);
						retcsrptr = remqh((que_ent_ptr_t)((sm_uc_ptr_t)&csr->state_que
							+ csr->state_que.bl));
						if ((cache_state_rec_ptr_t)retcsrptr != csr)
                                                {       /* Did not get the csr we intended so something must be wrong with cache.
                                                         * Kill -9 can cause this. Assert that we were doing a crash shutdown.
                                                         */
							assert(gtm_white_box_test_case_enabled
								&& (WBTEST_CRASH_SHUTDOWN_EXPECTED
								== gtm_white_box_test_case_number));
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							ret_value = ERR_DBCCERR;
							break;
						}
						csr->state_que.bl = (sm_off_t)0;
						csr->state_que.fl = (sm_off_t)0;
					} else
						/* The entry is still in the active queue waiting to be inserted into the wip
						 * queue.
						*/
						csr = NULL;
					rel_latch(&whead->latch);
				} else
					csr = NULL; /* did not get the lock */
				--fast_lock_count;
				assert(0 <= fast_lock_count);
			}
			/* else cr2flush is either in the active queue or in the free queue (i.e. not dirty).
			 * In either case, we cannot handle it in this function so return.
			 */
		} else
		{
			csr = (cache_state_rec_ptr_t)REMQHI((que_head_ptr_t)whead);
			if (INTERLOCK_FAIL == (INTPTR_T)csr)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtfini_lckfail1);
				ret_value = ERR_DBCCERR;
				break;
			}
		}
		if (NULL == csr)
			break;		/* empty queue */
		assert(!multi_proc_in_use);	/* wcs_wtstart uses syncio for online/offline rollback/recover forward phase */
		/* wcs_get_space relies on the fact that a cache-record that is out of either active or wip queue has its
		 * fl and bl fields set to 0. REMQHI would have already set them to 0. Assert that.
		 */
		assert(0 == csr->state_que.fl);
		assert(0 == csr->state_que.bl);
		if (csr == start_csr)
		{
			status = INSQHI((que_ent_ptr_t)csr, (que_head_ptr_t)whead);
			if (INTERLOCK_FAIL == status)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtfini_lckfail2);
				ret_value = ERR_DBCCERR;
			}
			break;		/* looped the queue */
		}
#		ifdef DEBUG
		cr = (cache_rec_ptr_t)((sm_uc_ptr_t)csr - SIZEOF(cr->blkque));
		assert(cr >= cr_lo);
		assert(cr < cr_hi);
#		endif
		assert(csr->dirty);
		assert(CR_BLKEMPTY != csr->blk);
		AIO_SHIM_ERROR(&(csr->aiocb), aio_errno);
		assert(EBADF != aio_errno);
		if ((ENOSYS == aio_errno) || (EINVAL == aio_errno))
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("aio_error()"), CALLFROM, aio_errno);
		}
		restart_errno = 0;
		requeue = REQUEUE_TO_WIP;
		epid = csr->epid;
		if (EINPROGRESS == aio_errno)
		{
			if (do_is_proc_alive_check && (process_id != epid))
			{
				assert(SIZEOF(process_id) == SIZEOF(uint4));
				/* hashtab_int4 routines do not handle 0 value so bump return value from is_proc_alive
				 * (TRUE or FALSE) by 1 when storing and decrement by 1 after lookup.
				 */
				if (NULL != (tabent = lookup_hashtab_int4(&wtfini_pid_ht, (uint4 *)(&epid))))
					pid_alive = (boolean_t)(UINTPTR_T)tabent->value - 1;
				else
				{
					pid_alive = is_proc_alive(epid, 0);
					new_pid = add_hashtab_int4(&wtfini_pid_ht, (uint4 *)&epid,
								(void *)(UINTPTR_T)(pid_alive + 1), &tabent);
					assert(new_pid);
				}
				/* If pid that issued the original write is no longer alive, we do not know if the aiocb
				 * structure was fully initialized (e.g. aiocb.aio_nbytes, aiocb.aio_offset etc.) when the
				 * pid died or not. So we cannot safely issue a rewrite (i.e. LSEEKWRITEASYNCRESTART) which
				 * assumes these are initialized. Instead we need to issue a fresh write (LSEEKWRITEASYNCSTART).
				 * The only function capable of issuing this is "wcs_wtstart" so put this cr back in active queue.
				 */
				if (!pid_alive)
				{
					WCS_OPS_TRACE(csa, process_id, wcs_ops_wtfini1, cr->blk, GDS_ANY_ABS2REL(csa,cr),
							cr->dirty, dbg_wtfini_lcnt, epid);
					if (!csr->bt_index && !csr->in_cw_set)
					{	/* This is an older twin so we do not need the update anymore.
						 * For comment on why "csr->in_cw_set" needs to be additionally checked,
						 * see usage of "csr->in_cw_set" a little later for a descriptive comment.
						 */
						requeue = REQUEUE_TO_FREE;
						/* csr->epid = 0 will happen a little later as part of REQUEUE_TO_FREE handling */
					} else
					{
						requeue = REQUEUE_TO_ACTIVE;
						csr->epid = 0;	/* Clear this since the process that issued the write is dead */
					}
				}
			}
		} else
		{	/* Status is available for the I/O. Note: In case IO was canceled, "aio_return" will return -1. */
			/* aio_errno == 0, if the request completed successfully. */
			/* aio_errno  > 0, A positive error number, if the asynchronous I/O operation failed.
			 *	This is the same value that would have been stored in the errno variable in the
			 *	case of the corresponding synchronous "write()" call.
			 */
			assert(EINTR != aio_errno);
			AIO_SHIM_RETURN(&(csr->aiocb), aio_retval); /* get the return value of the i/o */
#			ifdef DEBUG
			/* Fake an error once in a while. But do not do that in AIX if we are inside "wcs_flu" as we
			 * have seen a lot of test failures because "wcs_flu" takes more than 1 minute to reflush
			 * the dirty cache-record after a fake-error inside "wcs_wtfini".
			 */
			if (FAKE_WTERROR_FREQUENCY == ++dbg_skip_wcs_wt_restart)
				dbg_skip_wcs_wt_restart = 0;
			if ((0 == dbg_skip_wcs_wt_restart) && (0 < aio_retval) AIX_ONLY(&& (cnl->wcsflu_pid != process_id)))
			{
				WCS_OPS_TRACE(csa, process_id, wcs_ops_wtfini2, cr->blk, GDS_ANY_ABS2REL(csa,cr),	\
					cr->dirty, dbg_wtfini_lcnt, aio_retval);
				aio_retval = 0;
			}
#			endif
			if (0 < aio_retval)
			{	/* async IO completed successfully with no errors */
				assert(0 == aio_errno);
				/* Mark this block as written */
				csr->needs_first_write = FALSE;
				/* We can move this csr from the WIP queue to the FREE queue now that the write is complete.
				 * There is one exception though. If the write of an OLDER twin completes fine (0 == csr->bt_index)
				 * AND if csr->in_cw_set is still non-zero, it implies PHASE2 commit is in progress for this csr
				 * concurrently by another pid and since "in_cw_set" is still non-zero, it implies the buffer is
				 * likely needed by that pid (e.g. secshr_db_clnup/wcs_recover to complete the flush of the
				 * before-image block to an online backup file (in case of an error in the midst of commit).
				 * In that case, we should NOT touch csr->blk so keep csr in the WIP queue until "in_cw_set" clears.
				 */
				assert(REQUEUE_TO_WIP == requeue);
				if (csr->bt_index || !csr->in_cw_set)
					requeue = REQUEUE_TO_FREE;
			} else
			{	/* aio_errno can be 0 if we faked an aio error (by setting "aio_retval = 0" above) OR
				 * if the process that was doing the AIO got killed (and so the OS decided to abandon the IO).
				 * Assert accordingly.
				 */
				assert((0 < aio_errno)
					|| ((0 == aio_errno) && (!dbg_skip_wcs_wt_restart || (epid && !is_proc_alive(epid, 0)))));
				WCS_OPS_TRACE(csa, process_id, wcs_ops_wtfini3, cr->blk, GDS_ANY_ABS2REL(csa,cr),	\
					cr->dirty, dbg_wtfini_lcnt, aio_errno);
				/* Now that the IO is complete with some sort of error, handle the asyncio like is done in
				 * wcs_wtstart for syncio. The only exception is ECANCELED which is because the async IO got
				 * canceled. In this case it is not a real IO error. Same with dbg-only aio_errno of 0 which
				 * is to test the "wcs_wt_restart" logic (nothing to do with "wcs_wterror").
				 */
				if ((ECANCELED != aio_errno) && (0 != aio_errno))
				{
					if (!ret_value)
						ret_value = aio_errno;
					wcs_wterror(reg, aio_errno);
				}
				/* If a NEWER twin has been formed (indicated by csr->bt_index being 0) and an error occurs in
				 * the write of the OLDER twin, one would be tempted to ignore that error, move the OLDER twin
				 * from the WIP queue to the FREE queue and focus on the NEWER twin. But it is possible the OLDER
				 * twin is BEFORE an EPOCH whereas the NEWER twin is AFTER. In that case, we need the OLDER twin
				 * flushed to disk to catch the state of the database as of the EPOCH. So we keep the OLDER twin
				 * in the WIP queue until its write completes.
				 */
				/* Reissue the IO */
				restart_errno = wcs_wt_restart(udi, csr);	/* sets "csr->epid" */
				if (SYNCIO_MORPH_SUCCESS == restart_errno)
					requeue = REQUEUE_TO_FREE;
				else if (!restart_errno && !csr->epid)
				{	/* Case where IO was not reissued (either because we did not have crit or because we did
					 * not have read-write access to db. Put it back in active queue.
					 */
					requeue = REQUEUE_TO_ACTIVE;
				}
			}
		}
		if (REQUEUE_TO_WIP == requeue)
		{
			status = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)whead);
			if (INTERLOCK_FAIL == status)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtfini_lckfail3);
				ret_value = ERR_DBCCERR;
				break;
			}
			if (NULL == start_csr)
				start_csr = csr;
		} else if (REQUEUE_TO_FREE == requeue)
		{
			csr->flushed_dirty_tn = csr->dirty;
			csr->dirty = 0;
			cnl->wcs_buffs_freed++;
			csr->epid = 0;
			SUB_ENT_FROM_WIP_QUE_CNT(cnl);
			ADD_ENT_TO_FREE_QUE_CNT(cnl);
			if (csr->twin)
				BREAK_TWIN(csr, csa);
			CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
		} else
		{
			assert(REQUEUE_TO_ACTIVE == requeue);
			CLEAR_BUFF_UPDATE_LOCK(csr, &cnl->db_latch);
			ahead = &csa->acc_meth.bg.cache_state->cacheq_active;
			REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, wcb_wtfini_lckfail4);
			if (INTERLOCK_FAIL == n)
			{
				ret_value = ERR_DBCCERR;
				break;
			}
			SUB_ENT_FROM_WIP_QUE_CNT(cnl);
			ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
			WCS_OPS_TRACE(csa, process_id, wcs_ops_wtfini4, cr->blk, GDS_ANY_ABS2REL(csa,cr), cr->dirty,	\
				dbg_wtfini_lcnt, epid);
		}
	}
	cnl->wtfini_in_prog = 0;
	wtfini_in_prog--;
	assert(0 <= wtfini_in_prog);
	if (0 > wtfini_in_prog)
		wtfini_in_prog = 0;
	assert(!ret_value || ENOSPC == ret_value || ERR_DBCCERR == ret_value);
	return ret_value;
}
