/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef DEBUG
# include "gtm_stdio.h"
#endif

#include "arit.h"
#include "gt_timer.h"
#include "mvalconv.h"
#include "op.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "rel_quant.h"
#include "mv_stent.h"
#include "find_mvstent.h"
#if defined(DEBUG)
# include "hashtab_mname.h"
# include "rtnhdr.h"
# include "stack_frame.h"
# include "io.h"
# include "wcs_sleep.h"
# include "wbox_test_init.h"
# include "gtmio.h"
# include "util.h"
# include "deferred_signal_handler.h"
#endif

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#include "sleep.h"
#include "time.h"

GBLREF	mv_stent	*mv_chain;
GBLREF	stack_frame	*frame_pointer;
GBLREF	uint4		dollar_trestart;
GBLREF	unsigned char	*restart_pc, *restart_ctxt;
GBLREF	volatile int4	outofband;

#define HANGSTR "HANG"

/*
 * ------------------------------------------
 * Hang the process for a specified time.
 *
 *	Goes to sleep for a positive value.
 *	Any caught signal will terminate the sleep
 *	following the execution of that signal's catching routine.
 *
 * 	The actual hang duration should be NO LESS than the specified
 * 	duration for specified durations greater than .001 seconds.
 * 	Certain applications depend on this assumption.
 *
 * Arguments:
 *	num - time to sleep
 *
 * Return:
 *	none
 * ------------------------------------------
 */
void op_hang(mval* num)
{
	int		ms;
	double		tmp;
	mv_stent	*mv_zintcmd;
	ABS_TIME	cur_time, end_time;
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ms = 0;
	MV_FORCE_NUM(num);
	if (num->mvtype & MV_INT)
	{
		if (0 < num->m[1])
		{
			assert(MV_BIAS >= 1000);	/* if formats change overflow may need attention */
			ms = num->m[1] * (1000 / MV_BIAS);
		}
	} else if (0 == num->sgn) 		/* if sign is not 0 it means num is negative */
	{
		tmp = mval2double(num) * (double)1000;
		ms = ((double)MAXPOSINT4 >= tmp) ? (int)tmp : (int)MAXPOSINT4;
		assert(0 <= ms);
	}
	if (ms)
	{
		if ((TREF(tpnotacidtime)).m[1] < ms)
			TPNOTACID_CHECK(HANGSTR);
#		ifdef _AIX	/* AIX times are a bit wobbly, so nudge any 1sec. hang aiming to landing in the next sec */
		if (1000 == ms)		/* some apps expect a HANG 1 (1000 MS) to land in the next second */
			ms +=2;		/* push a bit (in milliseconds) */
#		endif
#		if defined(DEBUG)
		if (WBTEST_ENABLED(WBTEST_DEFERRED_TIMERS) && (3 > gtm_white_box_test_case_count) && (123000 == ms))
		{	/* LONG_SLEEP messes with signals */
			DEFER_INTERRUPTS(INTRPT_NO_TIMER_EVENTS, prev_intrpt_state);
			DBGFPF((stderr, "OP_HANG: will sleep for 20 seconds\n"));
			LONG_SLEEP(20);
			DBGFPF((stderr, "OP_HANG: done sleeping\n"));
			ENABLE_INTERRUPTS(INTRPT_NO_TIMER_EVENTS, prev_intrpt_state);
			return;
		}
		if (WBTEST_ENABLED(WBTEST_BREAKMPC)&& (0 == gtm_white_box_test_case_count) && (999 == ms))
		{
			frame_pointer->old_frame_pointer->mpc = (unsigned char *)GTM64_ONLY(0xdeadbeef12345678)
				NON_GTM64_ONLY(0xdead1234);
			return;
		}
		if (WBTEST_ENABLED(WBTEST_UTIL_OUT_BUFFER_PROTECTION) && (0 == gtm_white_box_test_case_count) && (999 == ms))
		{	/* Upon seeing a .999s hang this white-box test launches a timer that pops with a period of
		 	 * UTIL_OUT_SYSLOG_INTERVAL and prints a long message via util_out_ptr.
			 */
			start_timer((TID)&util_out_syslog_dump, UTIL_OUT_SYSLOG_INTERVAL, util_out_syslog_dump, 0, NULL);
			return;
		}
#		endif
		/* WARNING: This is not the same as sys_get_curr_time() which returns time from the monotonic clock */
		sys_get_wall_time(&cur_time);
		mv_zintcmd = find_mvstent_cmd(ZINTCMD_HANG, frame_pointer->restart_pc, frame_pointer->restart_ctxt, FALSE);
		if (!mv_zintcmd)
			add_int_to_abs_time(&cur_time, ms, &end_time);
		else
		{
			end_time = mv_zintcmd->mv_st_cont.mvs_zintcmd.end_or_remain;
			cur_time = sub_abs_time(&end_time, &cur_time);	/* get remaing time to sleep */
			if (0 <= cur_time.at_sec)
				ms = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
					    /* Round up in order to prevent premautre timeouts */
					    DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
			else
				ms = 0;		/* all done */
			/* restore/pop previous zintcmd_active[ZINTCMD_HANG] hints */
			TAREF1(zintcmd_active, ZINTCMD_HANG).restart_pc_last = mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_pc_prior;
			TAREF1(zintcmd_active, ZINTCMD_HANG).restart_ctxt_last
				= mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_ctxt_prior;
			TAREF1(zintcmd_active, ZINTCMD_HANG).count--;
			assert(0 <= TAREF1(zintcmd_active, ZINTCMD_HANG).count);
			if (mv_chain == mv_zintcmd)
				POP_MV_STENT();	/* just pop if top of stack */
			else
			{	/* flag as not active */
				mv_zintcmd->mv_st_cont.mvs_zintcmd.command = ZINTCMD_NOOP;
				mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_pc_check = NULL;
			}
			if (0 == ms)
				return;		/* done HANGing */
		}
		hiber_start_wall_time(ms);
	} else	/* the rel_quant below seems legitimate */
		rel_quant();
	if (outofband)
	{
		PUSH_MV_STENT(MVST_ZINTCMD);
		mv_chain->mv_st_cont.mvs_zintcmd.end_or_remain = end_time;
		mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_check = frame_pointer->restart_ctxt;
		mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_check = frame_pointer->restart_pc;
		/* save current information from zintcmd_active */
		mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_prior = TAREF1(zintcmd_active, ZINTCMD_HANG).restart_ctxt_last;
		mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_prior = TAREF1(zintcmd_active, ZINTCMD_HANG).restart_pc_last;
		TAREF1(zintcmd_active, ZINTCMD_HANG).restart_pc_last = frame_pointer->restart_pc;
		TAREF1(zintcmd_active, ZINTCMD_HANG).restart_ctxt_last = frame_pointer->restart_ctxt;
		TAREF1(zintcmd_active, ZINTCMD_HANG).count++;
		mv_chain->mv_st_cont.mvs_zintcmd.command = ZINTCMD_HANG;
		async_action(FALSE);
	}
	return;
}
