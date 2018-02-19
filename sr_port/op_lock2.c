/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "outofband.h"
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
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#include "lockdefs.h"
#include "is_proc_alive.h"
#include "mvalconv.h"
#include "min_max.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

GBLREF	unsigned char	cm_action;
GBLREF	uint4		dollar_tlevel;
GBLREF	uint4		dollar_trestart;
GBLREF	unsigned short	lks_this_cmd;
GBLREF	mlk_pvtblk	*mlk_pvt_root;
GBLREF	mlk_stats_t	mlk_stats;			/* Process-private M-lock statistics */
GBLREF	mv_stent	*mv_chain;
GBLREF	int4		outofband;
GBLREF	bool		out_of_time;
GBLREF	uint4		process_id;
GBLREF	bool		remlkreq;
GBLREF	unsigned char	*restart_ctxt, *restart_pc;
GBLREF	unsigned int	t_tries;

error_def(ERR_LOCKINCR2HIGH);
error_def(ERR_LOCKIS);
error_def(ERR_LOCKTIMINGINTP);

#define LOCKTIMESTR "LOCK"
#define ZALLOCTIMESTR "ZALLOCATE"
#define MAX_WARN_STR_ARG_LEN 256

/* We made these messages seperate functions because we did not want to do the MAXSTR_BUFF_DECL(buff) declaration in op_lock2,
 * because  MAXSTR_BUFF_DECL macro would allocate a huge stack every time op_lock2 is called.
 */
STATICFNDCL void level_err(mlk_pvtblk *pvt_ptr);  /* These definitions are made here because there is no appropriate place to */
STATICFNDCL void tp_warning(mlk_pvtblk *pvt_ptr); /* put these prototypes. These will not be used anywhere else so we did not
						   * want to create a op_lock2.h just for these functions.
						   */
STATICFNDCL void level_err(mlk_pvtblk *pvt_ptr)
{
	lks_this_cmd = 0;
	MAXSTR_BUFF_DECL(buff);
	MAXSTR_BUFF_INIT;
	lock_str_to_buff(pvt_ptr, buff, MAX_STRBUFF_INIT);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_LOCKINCR2HIGH, 1, pvt_ptr->level, ERR_LOCKIS, 2, LEN_AND_STR(buff));
}
STATICFNDCL void tp_warning(mlk_pvtblk *pvt_ptr)
{
	MAXSTR_BUFF_DECL(buff);
	mval		zpos;

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
int	op_lock2(mval *timeout, unsigned char laflag)	/* timeout is in milliseconds */
{
	boolean_t		blocked, timer_on;
	signed char		gotit;
	unsigned short		locks_bckout, locks_done;
	int4			msec_timeout;	/* timeout in milliseconds */
	mlk_pvtblk		*pvt_ptr1, *pvt_ptr2, **prior, *already_locked;
	unsigned char		action;
	ABS_TIME		cur_time, end_time, remain_time;
	mv_stent		*mv_zintcmd;
#	ifdef VMS
	int4			status;			/* needed for BLOCKING_PROC_DEAD macro in VMS */
	int4			icount, time[2];	/* needed for BLOCKING_PROC_DEAD macro in VMS */
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gotit = -1;
	cm_action = laflag;
	out_of_time = timer_on = FALSE;
	if (CM_ZALLOCATES == cm_action)		/* can't use ? : syntax here because of the way the macros nest */
		MV_FORCE_MSTIMEOUT(timeout, msec_timeout, ZALLOCTIMESTR);
	else
		MV_FORCE_MSTIMEOUT(timeout, msec_timeout, LOCKTIMESTR);
	if (NO_M_TIMEOUT != msec_timeout)
	{
		if (0 == msec_timeout)
			out_of_time = TRUE;
		else
		{
			sys_get_curr_time(&cur_time);
			mv_zintcmd = find_mvstent_cmd(ZINTCMD_LOCK, restart_pc, restart_ctxt, FALSE);
			if (mv_zintcmd)
			{
				end_time = mv_zintcmd->mv_st_cont.mvs_zintcmd.end_or_remain;
				remain_time = sub_abs_time(&end_time, &cur_time);	/* get remaing time to sleep */
				if (0 <= remain_time.at_sec)
					msec_timeout = (int4)((remain_time.at_sec * MILLISECS_IN_SEC) +
						/* Round up in order to prevent premature timeouts */
						DIVIDE_ROUND_UP(remain_time.at_usec, MICROSECS_IN_MSEC));
				else
					msec_timeout = 0;
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
				add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			if (0 < msec_timeout)
			{
				start_timer((TID)mlk_lock, msec_timeout, wake_alarm, 0, NULL);
				timer_on = TRUE;
			} else
			{
				cancel_timer((TID)mlk_lock);
				out_of_time = TRUE;
			}
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
				gotit = gvcmx_resremlk(cm_action);
			else
				gotit = gvcmx_reqremlk(cm_action, msec_timeout);	/* REQIMMED if 2nd arg == 0 */
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
		{	/* Go thru the list of all locks to be obtained attempting to lock
			 * each one. If any lock could not be obtained, break out of the loop
			 * If the lock is already obtained, then skip that lock.
			 */
			if ((pvt_ptr1 == already_locked) || !mlk_lock(pvt_ptr1, 0, TRUE))
			{	/* If lock is obtained */
				pvt_ptr1->granted = TRUE;
				switch (laflag)
				{
				case CM_LOCKS:
					pvt_ptr1->level = 1;
					break;
				case INCREMENTAL:
					if (pvt_ptr1->level < 511) /* The same lock can not be incremented more than 511 times. */
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
			{
				blocked = TRUE;
				break;
			}
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
		case INCREMENTAL:
		case CM_ZALLOCATES:
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
		if (dollar_tlevel && msec_timeout && (CDB_STAGNATE <= t_tries))
		{
			assert(have_crit(CRIT_HAVE_ANY_REG));
			tp_warning(pvt_ptr2);
		}
		for (;;)
		{
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
					if (outofband  && !out_of_time)
					{
						if (timer_on)
						{
							cancel_timer((TID)mlk_lock);
							timer_on = FALSE;
						}
						if (NO_M_TIMEOUT != msec_timeout)
						{	/* get remain = end_time - cur_time */
							sys_get_curr_time(&cur_time);
							remain_time = sub_abs_time(&end_time, &cur_time);
							msec_timeout = (int4)(remain_time.at_sec * MILLISECS_IN_SEC +
								/* Round up in order to prevent premature timeouts */
								DIVIDE_ROUND_UP(remain_time.at_usec, MICROSECS_IN_MSEC));
							if (0 >= msec_timeout)
							{
								out_of_time = TRUE;
								break;
							}
							if ((tptimeout != outofband) && (ctrlc != outofband))
							{
								PUSH_MV_STENT(MVST_ZINTCMD);
								mv_chain->mv_st_cont.mvs_zintcmd.end_or_remain = end_time;
								mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_check = restart_ctxt;
								mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_check = restart_pc;
								/* save current information from zintcmd_active */
								mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_prior
									= TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_ctxt_last;
								mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_prior
									= TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_pc_last;
								TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_pc_last = restart_pc;
								TAREF1(zintcmd_active, ZINTCMD_LOCK).restart_ctxt_last
									= restart_ctxt;
								TAREF1(zintcmd_active, ZINTCMD_LOCK).count++;
								mv_chain->mv_st_cont.mvs_zintcmd.command = ZINTCMD_LOCK;
							}
						}
						lks_this_cmd = 0;	/* no return - clear flag that we're in LOCK processing */
						outofband_action(FALSE);
					}
					break;
				}
			}
			/* Sleep first before reattempting a blocked lock. Note: this is used by the lock fairness algorithm
			 * in mlk_shrblk_find. If mlk_lock is invoked for the second (or higher) time in op_lock2 for the
			 * same lock resource, "mlk_shrblk_find" assumes a sleep has happened in between two locking attempts.
			 */
			hiber_start_wait_any(LOCK_SELF_WAKE);
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
				continue;
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
	if (NO_M_TIMEOUT != msec_timeout)
	{	/* was timed or immediate */
		if (timer_on)
			cancel_timer((TID)mlk_lock);
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

