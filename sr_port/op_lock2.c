/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cdb_sc.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gt_timer.h"
#include "gtm_threadgbl.h"
#include "iotimer.h"
#include "locklits.h"
#include "mlkdef.h"
#include "t_retry.h"
#include "mlk_lock.h"
#include "mlk_bckout.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"
#include "mlk_unpend.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "mlk_check_own.h"
#include "lock_str_to_buff.h"
#include "gvcmx.h"
#include "gvcmz.h"
#include "rel_quant.h"
#include "lckclr.h"
#include "wake_alarm.h"
#include "op.h"
#include "mv_stent.h"
#include "find_mvstent.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "gtm_maxstr.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "io.h"
#include "iott_setterm.h"
#include "jnl.h"
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "getzposition.h"
#include "lockdefs.h"
#include "is_proc_alive.h"
#include "mvalconv.h"
#include "min_max.h"
#include "is_equ.h"		/* for MV_FORCE_NSTIMEOUT macro */

GBLREF	bool		out_of_time, remlkreq;
GBLREF	mlk_pvtblk	*mlk_pvt_root;
GBLREF	mlk_stats_t	mlk_stats;			/* Process-private M-lock statistics */
GBLREF	mv_stent	*mv_chain;
GBLREF	stack_frame	*frame_pointer;
GBLREF	uint4		dollar_tlevel, dollar_trestart, process_id;
GBLREF	unsigned char	cm_action, *restart_ctxt, *restart_pc;
GBLREF	unsigned int	t_tries;
GBLREF	unsigned short	lks_this_cmd;
GBLREF	volatile int4	outofband;

error_def(ERR_LOCKINCR2HIGH);
error_def(ERR_LOCKIS);
error_def(ERR_LOCKTIMINGINTP);

#define LCKLEVELLMT		511
#define LOCKTIMESTR		"LOCK"
#define MAX_WARN_STR_ARG_LEN	256
#define ZALLOCTIMESTR		"ZALLOCATE"
#define LOCK_SELF_WAKE_START	1	/* sleep   1 msec at start before checking if wakeup was sent by lock holder */
#define LOCK_SELF_WAKE_MAX	128	/* sleep 128 msec at max before checking if wakeup was sent by lock holder */

/* We made these messages seperate functions because we did not want to do the MAXSTR_BUFF_DECL(buff) declaration in op_lock2,
 * because  MAXSTR_BUFF_DECL macro would allocate a huge stack every time op_lock2 is called.
 */
STATICFNDCL void level_err(mlk_pvtblk *pvt_ptr);  /* These definitions are made here because there is no appropriate place to */
STATICFNDCL void tp_warning(mlk_pvtblk *pvt_ptr); /* put these prototypes. These will not be used anywhere else so we did not
						   * want to create a op_lock2.h just for these functions.
						   */
STATICFNDCL void level_err(mlk_pvtblk *pvt_ptr)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	lks_this_cmd = 0;
	MAXSTR_BUFF_DECL(buff);
	MAXSTR_BUFF_INIT;
	lock_str_to_buff(pvt_ptr, buff, MAX_STRBUFF_INIT);
	RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_LOCKINCR2HIGH, 1, LCKLEVELLMT, ERR_LOCKIS, 2, LEN_AND_STR(buff));
}
STATICFNDCL void tp_warning(mlk_pvtblk *pvt_ptr)
{
	MAXSTR_BUFF_DECL(buff);
	mval		zpos;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	getzposition(&zpos);
	MAXSTR_BUFF_INIT;
	lock_str_to_buff(pvt_ptr, buff, MAX_STRBUFF_INIT);
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_LOCKTIMINGINTP, 2, zpos.str.len, zpos.str.addr,
		     ERR_LOCKIS, 2, LEN_AND_STR(buff));
}
/*
 * -----------------------------------------------
 * Arguments:
 *	timeout	- max. time to wait for locks before giving up
 *      laflag - passed to gvcmx* routines as "laflag" argument;
 *		 originally indicated the request was a Lock or
 *		 zAllocate request (hence the name "laflag"), but
 *		 now capable of holding more values signifying
 *		 additional information
 *
 * Return:
 *	1 - if not timeout specified
 *	if timeout specified:
 *		!= 0 - all the locks int the list obtained, or
 *		0 - blocked
 *	The return result is suited to be placed directly into
 *	the $T variable by the caller if timeout is specified.
 * -----------------------------------------------
 */
int	op_lock2(mval *timeout, unsigned char laflag)	/* timeout is in seconds */
{

	uint8			nsec_timeout;	/* timeout in nanoseconds */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cm_action = laflag;
	if (CM_ZALLOCATES == cm_action)		/* can't use ? : syntax here because of the way the macros nest */
		MV_FORCE_NSTIMEOUT(timeout, nsec_timeout, ZALLOCTIMESTR);
	else
		MV_FORCE_NSTIMEOUT(timeout, nsec_timeout, LOCKTIMESTR);
	return op_lock2_common(nsec_timeout, laflag);
}

/*
 * -----------------------------------------------
 * See op_lock2 above. op_lock2_common does the same thing as
 * op_lock2 except the timeout value passed to it is already
 * in nanoseconds.
 */
int	op_lock2_common(uint8 timeout, unsigned char laflag) /* timeout is in nanoseconds */
{
	boolean_t		blocked;
	signed char		gotit;
	unsigned short		locks_bckout, locks_done;
	mlk_pvtblk		*pvt_ptr1, *pvt_ptr2, **prior, *already_locked;
	unsigned char		action;
	ABS_TIME		cur_time, end_time, remain_time;
	mv_stent		*mv_zintcmd;
	uint4			sleep_msec;
	mlk_pvtctl_ptr_t	pctl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gotit = -1;
	cm_action = laflag;
	out_of_time = FALSE;
	if (NO_M_TIMEOUT != timeout)
	{
		if (0 == timeout)
			out_of_time = TRUE;
		else
		{
			sys_get_curr_time(&cur_time);
			mv_zintcmd = find_mvstent_cmd(ZINTCMD_LOCK, frame_pointer->restart_pc, frame_pointer->restart_ctxt, FALSE);
			if (mv_zintcmd)
			{
				end_time = mv_zintcmd->mv_st_cont.mvs_zintcmd.end_or_remain;
				remain_time = sub_abs_time(&end_time, &cur_time);	/* get remaing time to sleep */
				if ((0 <= remain_time.tv_sec) && (0 <= remain_time.tv_nsec))
					timeout = ((remain_time.tv_sec * (uint8)NANOSECS_IN_SEC) + remain_time.tv_nsec);
				else
					timeout = 0;
				TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_pc_last
					= mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_pc_prior;
				TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_ctxt_last
					= mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_ctxt_prior;
				TAREF1(zintcmd_active, ZINTCMD_LOCK).count--;
				assert(0 <= TAREF1(zintcmd_active, ZINTCMD_LOCK).count);
				if (mv_chain == mv_zintcmd)
					POP_MV_STENT(); /* just pop if top of stack */
				else
				{       /* flag as not active */
					mv_zintcmd->mv_st_cont.mvs_zintcmd.command = ZINTCMD_NOOP;
					mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_pc_check = NULL;
				}
			} else
				add_uint8_to_abs_time(&cur_time, timeout, &end_time);
		}
	}
	lckclr();
	TREF(mlk_yield_pid) = 0;
	already_locked = NULL;
	for (blocked = FALSE;  !blocked;)
	{	/* if this is a request for a remote node */
		if (remlkreq)
		{
			if (gotit >= 0)
				gotit = gvcmx_resremlk(cm_action, timeout, &end_time);
			else
				gotit = gvcmx_reqremlk(cm_action, timeout, &end_time);	/* REQIMMED if 2nd arg == 0 */
			if (!gotit)
			{	/* only REQIMMED returns false */
				blocked = TRUE;
				break;
			}
		}
		/* If we gave up the fairness algorithm at least once during this invocation of op_lock2(), continue with that until
		 * the end of op_lock2()
		 */
		for (pvt_ptr1 = mlk_pvt_root, locks_done = 0;  locks_done < lks_this_cmd;  pvt_ptr1 = pvt_ptr1->next, locks_done++)
		{	/* Go thru the list of all locks to be obtained attempting to lock each one. If any lock could not be
			 * obtained (and cannot be retried immediately), break out of the loop. If the lock is already obtained,
			 * then skip that lock.
			 */
			while (TRUE)
			{
				pctl = &pvt_ptr1->pvtctl;
				pctl->gc_needed = FALSE;	/* Initialize flags for this lock pass - Can be set to TRUE.. */
				pctl->rehash_needed = FALSE;	/* .. in mlk_shrhash_find_bucket.c */
				pctl->resize_needed = FALSE;
				if ((pvt_ptr1 == already_locked) || !mlk_lock(pvt_ptr1, 0, TRUE))
				{	/* If lock is obtained */
					pvt_ptr1->granted = TRUE;
					switch (laflag)
					{
						case CM_LOCKS:
							pvt_ptr1->level = 1;
							break;
						case INCREMENTAL:
							if (LCKLEVELLMT >= (pvt_ptr1->level + pvt_ptr1->translev))
								/* The same lock can not be incremented more than 511 times. */
								pvt_ptr1->level += pvt_ptr1->translev;
							else
								level_err(pvt_ptr1);
							break;
						case CM_ZALLOCATES:
							pvt_ptr1->zalloc = TRUE;
							break;
						default:
							assertpro(FALSE && laflag);
							break;
					}
				} else
				{	/* Lock was not obtained - see if we are eligible to immediately retry this lock. The
					 * required conditions are:
					 *   1. One of pctl->gc_needed || pctl->rehash_needed || pctl->resize_needed} is set.
					 *   2. pctl->hash_fail_cnt is less than or equal to MAX_LOCK_GC_REHASH_RESIZE_RETRYS.
					 * If not, normal lock-blocked processing happens (sleep a bit before retry)..
					 */
					if ((pctl->gc_needed || pctl->rehash_needed || pctl->resize_needed)
					    && (MAX_LOCK_GC_REHASH_RESIZE_RETRYS >= pctl->hash_fail_cnt))
						/* Conditions met for a retry */
						continue;
					/* Else do a retry after some cleanup and a short nap */
					blocked = TRUE;
				}
				break;
			}
			if (blocked)
				break;
		}
		/* If we did not get blocked, we are all done */
		if (!blocked)
			break;
		/* We got blocked and need to keep retrying after some time interval */
		if (remlkreq)
			gvcmx_susremlk(cm_action);
		switch (cm_action)
		{
		case CM_LOCKS:
			action = LOCKED;
			break;
		case CM_ZALLOCATES:
			action = ZALLOCATED;
			break;
		case INCREMENTAL:
			action = cm_action;
			break;
		default:
			assertpro(FALSE && cm_action);
			break;
		}
		for (pvt_ptr2 = mlk_pvt_root, locks_bckout = 0;  locks_bckout < locks_done;
			pvt_ptr2 = pvt_ptr2->next, locks_bckout++)
		{
			assert(pvt_ptr2->granted && (pvt_ptr2 != pvt_ptr1));
			mlk_bckout(pvt_ptr2, action);
		}
		assert(!pvt_ptr2->granted && (pvt_ptr2 == pvt_ptr1));
		if (dollar_tlevel && timeout && (CDB_STAGNATE <= t_tries))
		{
			assert(have_crit(CRIT_HAVE_ANY_REG));
			tp_warning(pvt_ptr2);
		}
		sleep_msec = LOCK_SELF_WAKE_START;	/* start at LOCK_SELF_WAKE_START msec and double it
							 * upto LOCK_SELF_WAKE_MAX msec and then cycle back.
							 */
		for (;;)
		{
			SET_OUT_OF_TIME_IF_APPROPRIATE(timeout, &end_time, out_of_time);	/* may set "out_of_time" */
			assert((!(SFF_INDCE & frame_pointer->flags)) || (frame_pointer->restart_ctxt == frame_pointer->ctxt));
			if (out_of_time || outofband)
			{	/* if time expired || control-c, tptimeout, or jobinterrupt encountered */
				if (outofband || !mlk_check_own(pvt_ptr1))
				{	/* If CTL-C, check lock owner */
					if (pvt_ptr1->nodptr)		/* Get off pending list to be sent a wake */
						mlk_unpend(pvt_ptr1);
					/* Cancel all remote locks obtained so far */
					if (remlkreq)
					{
						gvcmx_canremlk();
						gvcmz_clrlkreq();
						remlkreq = FALSE;
					}
					if (outofband && !out_of_time)
					{
						SET_OUT_OF_TIME_IF_APPROPRIATE(timeout, &end_time, out_of_time);
								/* may set "out_of_time" */
						if (out_of_time)
							break;
						if (NO_M_TIMEOUT != timeout)
						{
							if ((tptimeout != outofband) && (ctrlc != outofband))
							{
								PUSH_MV_STENT(MVST_ZINTCMD);
								mv_chain->mv_st_cont.mvs_zintcmd.end_or_remain = end_time;
								mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_check
									= frame_pointer->restart_ctxt;
								mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_check
									= frame_pointer->restart_pc;
								/* save current information from zintcmd_active */
								mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_prior
									= TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_ctxt_last;
								mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_prior
									= TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_pc_last;
								TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_pc_last
									= frame_pointer->restart_pc;
								TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_ctxt_last
									= frame_pointer->restart_ctxt;
								TAREF1(zintcmd_active, ZINTCMD_LOCK).count++;
								mv_chain->mv_st_cont.mvs_zintcmd.command = ZINTCMD_LOCK;
							}
						}
						lks_this_cmd = 0;	/* no return - clear flag that we're in LOCK processing */
						async_action(FALSE);
					}
					break;
				}
			}
			/* Sleep first before reattempting a blocked lock. Note: this is used by the lock fairness algorithm
			 * in mlk_shrblk_find. If mlk_lock is invoked for the second (or higher) time in op_lock2 for the
			 * same lock resource, "mlk_shrblk_find" assumes a sleep has happened in between two locking attempts.
			 */
			UPDATE_CRIT_COUNTER(pvt_ptr1->pvtctl.csa, WS_39);
			hiber_start_wait_any(sleep_msec);
			sleep_msec = sleep_msec * 2;
			if (LOCK_SELF_WAKE_MAX <= sleep_msec)
				sleep_msec = LOCK_SELF_WAKE_START;
			/* Every reattempt at a blocking lock needs crit which could be a bottleneck. So minimize reattempts.
			 * The "blk_sequence" check below serves that purpose. If the sequence number is different between
			 * the shared and private copies, it means the lock state in shared memory has changed since last we
			 * did our blocking mlk_lock and so it is time to reattempt. But if the sequence numbers are the same,
			 * we dont need to reattempt. That said, we still need to check if the blocking pid is still alive
			 * and if so we continue to sleep. If not, we reattempt the lock in case the holder pid was kill -9ed.
			 * If pvt_ptr1->blocked is NULL, it implies there is not enough space in lock shm so mlk_shrblk_find
			 * returned blocked = TRUE. In this case, there is no "pvt_ptr1->blocked" to do the sequence number
			 * check so keep reattempting the lock.
			 */
			if ((NULL != pvt_ptr1->blocked)
					&& (pvt_ptr1->blk_sequence == pvt_ptr1->blocked->sequence)
					&& (!BLOCKING_PROC_DEAD(pvt_ptr1, time, icount, status)))
			{
				if (pvt_ptr1->pvtctl.ctl->lock_gc_in_progress.u.parts.latch_pid == process_id)
					pvt_ptr1->pvtctl.ctl->lock_gc_in_progress.u.parts.latch_pid = 0;
				continue;
			}
			/* Note that "TREF(mlk_yield_pid)" is not initialized here as we want to use any value inherited
			 * from previous calls to mlk_lock for this lock.
			 */
			if (!mlk_lock(pvt_ptr1, 0, FALSE))
			{	/* If we got the lock, break out of timer loop */
				blocked = FALSE;
				if (MLK_FAIRNESS_DISABLED != TREF(mlk_yield_pid))
					TREF(mlk_yield_pid) = 0; /* Allow yielding for the other locks */
				if (pvt_ptr1 != mlk_pvt_root)
				{	/* in the absence of contrary evidence this re_quant seems legitimate */
					rel_quant();		/* attempt to get a full timeslice for maximum chance to get all */
					mlk_unlock(pvt_ptr1);
					already_locked = NULL;
				} else
					already_locked = pvt_ptr1;
				break;
			}
			if (pvt_ptr1->nodptr)
				mlk_check_own(pvt_ptr1);		/* clear an abandoned owner */
		}
		if (blocked && out_of_time)
			break;
		if (locks_bckout)
			TREF(mlk_yield_pid) = MLK_FAIRNESS_DISABLED; /* Disable fairness to avoid livelocks */
	}
	if (remlkreq)
	{
		gvcmz_clrlkreq();
		remlkreq = FALSE;
	}
	lks_this_cmd = 0;	/* reset so we can check whether an extrinsic is trying to nest a LOCK operation */
	if (NO_M_TIMEOUT != timeout)
	{	/* was timed or immediate */
		if (blocked)
		{
			for (prior = &mlk_pvt_root;  *prior;)
			{
				if (!(*prior)->granted)
				{	/* if entry was never granted, delete list entry */
					mlk_pvtblk_delete(prior);
				} else
					prior = &((*prior)->next);
			}
			mlk_stats.n_user_locks_fail++;
			return (FALSE);
		}
	}
	mlk_stats.n_user_locks_success++;
	return (TRUE);
}
