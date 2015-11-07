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
#include "lk_check_own.h"
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

#define LOCKTIMESTR "LOCK time too long"
#define ZALLOCTIMESTR "ZALLOCATE time too long"

/* We made this a error seperate function because we did not wanted to do the MAXSTR_BUFF_DECL(buff) declartion in op_lock2,
 * because  MAXSTR_BUFF_DECL macro would allocate a huge stack every time op_lock2 is called.
 */
STATICFNDCL void level_err(mlk_pvtblk *pvt_ptr); /* This definition is made here because there is no appropriate place to
						  * put this prototype. This will not be used anywhere else so we did not
						  * wanted to create a op_lock2.h just for this function.
						  */
STATICFNDCL void level_err(mlk_pvtblk *pvt_ptr)
{
	MAXSTR_BUFF_DECL(buff);
	MAXSTR_BUFF_INIT;
	lock_str_to_buff(pvt_ptr, buff, MAX_STRBUFF_INIT);
	rts_error(VARLSTCNT(7) ERR_LOCKINCR2HIGH, 1, pvt_ptr->level, ERR_LOCKIS, 2, LEN_AND_STR(buff));
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
int	op_lock2(int4 timeout, unsigned char laflag)	/* timeout is in seconds */
{
	boolean_t	blocked, timer_on;
	signed char	gotit;
	unsigned short	locks_bckout, locks_done;
	int4		msec_timeout;	/* timeout in milliseconds */
	mlk_pvtblk	*pvt_ptr1, *pvt_ptr2, **prior, *already_locked;
	unsigned char	action;
	ABS_TIME	cur_time, end_time, remain_time;
	mv_stent	*mv_zintcmd;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gotit = -1;
	cm_action = laflag;
	out_of_time = FALSE;
	if (timeout < 0)
		timeout = 0;
	else if (TREF(tpnotacidtime) < timeout)
	{
		if (CM_ZALLOCATES == cm_action)
			TPNOTACID_CHECK(ZALLOCTIMESTR)
		else
			TPNOTACID_CHECK(LOCKTIMESTR)
	}
	if (!(timer_on = (NO_M_TIMEOUT != timeout)))	/* NOTE assignment */
		msec_timeout = NO_M_TIMEOUT;
	else
	{
		msec_timeout = timeout2msec(timeout);
		if (0 == msec_timeout)
		{
			out_of_time = TRUE;
			timer_on = FALSE;
		} else
		{
			mv_zintcmd = find_mvstent_cmd(ZINTCMD_LOCK, restart_pc, restart_ctxt, FALSE);
			if (mv_zintcmd)
			{
				remain_time = mv_zintcmd->mv_st_cont.mvs_zintcmd.end_or_remain;
				if (0 <= remain_time.at_sec)
					msec_timeout = (int4)(remain_time.at_sec * 1000 + remain_time.at_usec / 1000);
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
			}
			if (0 < msec_timeout)
			{
				sys_get_curr_time(&cur_time);
				add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
				start_timer((TID)&timer_on, msec_timeout, wake_alarm, 0, NULL);
			} else
			{
				out_of_time = TRUE;
				timer_on = FALSE;
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
					GTMASSERT;
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
			GTMASSERT;
			break;
		}
		for (pvt_ptr2 = mlk_pvt_root, locks_bckout = 0;  locks_bckout < locks_done;
			pvt_ptr2 = pvt_ptr2->next, locks_bckout++)
		{
			assert(pvt_ptr2->granted && (pvt_ptr2 != pvt_ptr1));
			mlk_bckout(pvt_ptr2, action);
		}
		if (dollar_tlevel && (CDB_STAGNATE <= t_tries))
		{	/* upper TPNOTACID_CHECK conditioned on no short timeout; this one rel_crits to avoid potential deadlock */
			assert(TREF(tpnotacidtime) >= timeout);
			if (CM_ZALLOCATES == action)
				TPNOTACID_CHECK(ZALLOCTIMESTR)
			else
				TPNOTACID_CHECK(LOCKTIMESTR)
		}
		for (;;)
		{
			if (out_of_time || outofband)
			{	/* if time expired || control-c, tptimeout, or jobinterrupt encountered */
				if (outofband || !lk_check_own(pvt_ptr1))
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
							cancel_timer((TID)&timer_on);
							timer_on = FALSE;
						}
						if (NO_M_TIMEOUT != timeout)
						{	/* get remain = end_time - cur_time */
							sys_get_curr_time(&cur_time);
							remain_time = sub_abs_time(&end_time, &cur_time);
							if (0 <= remain_time.at_sec)
								msec_timeout = (int4)(remain_time.at_sec * 1000
									+ remain_time.at_usec / 1000);
							else
								msec_timeout = 0;	/* treat as out_of_time */
							if (0 >= msec_timeout)
							{
								out_of_time = TRUE;
								timer_on = FALSE;	/* as if LOCK :0 */
								break;
							}
							if ((tptimeout != outofband) && (ctrlc != outofband))
							{
								PUSH_MV_STENT(MVST_ZINTCMD);
								mv_chain->mv_st_cont.mvs_zintcmd.end_or_remain = remain_time;
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
							outofband_action(FALSE);	/* no return */
						} else
							outofband_action(FALSE);	/* no return */
					}
					break;
				}
			}
			/* Sleep first before reattempting a blocked lock. Note: this is used by the lock fairness algorithm
			 * in mlk_shrblk_find. If mlk_lock is invoked for the second (or higher) time in op_lock2 for the
			 * same lock resource, "mlk_shrblk_find" assumes a sleep has happened in between two locking attempts.
			 */
			hiber_start_wait_any(LOCK_SELF_WAKE);
			/* Note that "TREF(mlk_yield_pid)" is not initialized here as we want to use any value inherited
			 * from previous calls to mlk_lock for this lock.
			 */
			if (!mlk_lock(pvt_ptr1, 0, FALSE))
			{	/* If we got the lock, break out of timer loop */
				blocked = FALSE;
				if (MLK_FAIRNESS_DISABLED != TREF(mlk_yield_pid))
					TREF(mlk_yield_pid) = 0; /* Allow yielding for the other locks */
				if (pvt_ptr1 != mlk_pvt_root)
				{
					rel_quant();		/* attempt to get a full timeslice for maximum chance to get all */
					mlk_unlock(pvt_ptr1);
					already_locked = NULL;
				} else
					already_locked = pvt_ptr1;
				break;
			}
			if (pvt_ptr1->nodptr)
				lk_check_own(pvt_ptr1);		/* clear an abandoned owner */
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
	if (NO_M_TIMEOUT != timeout)
	{	/* was timed or immediate */
		if (timer_on && !out_of_time)
			cancel_timer((TID)&timer_on);
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

