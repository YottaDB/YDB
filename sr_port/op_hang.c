/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
# include <ssdef.h>
# include "efn.h"
# include "timedef.h"
#endif
#ifdef DEBUG
# include "gtm_stdio.h"
#endif

#include "arit.h"
#include "gt_timer.h"
#include "mvalconv.h"
#include "op.h"
#include "outofband.h"
#include "rel_quant.h"
#include "mv_stent.h"
#include "find_mvstent.h"
#if defined(DEBUG) && defined(UNIX)
# include "hashtab_mname.h"
# include "rtnhdr.h"
# include "stack_frame.h"
# include "io.h"
# include "wcs_sleep.h"
# include "wbox_test_init.h"
# include "gtmio.h"
# include "have_crit.h"
# include "deferred_signal_handler.h"
#endif

GBLREF	int4		outofband;
GBLREF	unsigned char	*restart_pc, *restart_ctxt;
GBLREF	mv_stent	*mv_chain;
#ifdef DEBUG
GBLREF	stack_frame	*frame_pointer;
#endif

error_def(ERR_SYSCALL);

/*
 * ------------------------------------------
 * Hang the process for a specified time.
 *
 *	Goes to sleep for a positive value.
 *	Any caught signal will terminate the sleep
 *	following the execution of that signal's catching routine.
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
	mv_stent	*mv_zintcmd;
	ABS_TIME	cur_time, end_time;
#	ifdef VMS
	uint4 		time[2];
	int4		efn_mask, status;
#	endif
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
		ms = mval2i(num) * 1000;	/* too big to care about fractional amounts */
	if (ms)
	{
#		if defined(DEBUG) && defined(UNIX)
		if (gtm_white_box_test_case_enabled
			&& (WBTEST_DEFERRED_TIMERS == gtm_white_box_test_case_number)
			&& (3 > gtm_white_box_test_case_count)
			&& (123000 == ms))
		{
			DEFER_INTERRUPTS(INTRPT_NO_TIMER_EVENTS);
			DBGFPF((stderr, "OP_HANG: will sleep for 20 seconds\n"));
			LONG_SLEEP(20);
			DBGFPF((stderr, "OP_HANG: done sleeping\n"));
			ENABLE_INTERRUPTS(INTRPT_NO_TIMER_EVENTS);
			return;
		}
		if (gtm_white_box_test_case_enabled
			&& (WBTEST_BREAKMPC == gtm_white_box_test_case_number)
			&& (0 == gtm_white_box_test_case_count)
			&& (999 == ms))
		{
			frame_pointer->old_frame_pointer->mpc = (unsigned char *)GTM64_ONLY(0xdeadbeef12345678)
				NON_GTM64_ONLY(0xdead1234);
			return;
		}
#		endif
		sys_get_curr_time(&cur_time);
		mv_zintcmd = find_mvstent_cmd(ZINTCMD_HANG, restart_pc, restart_ctxt, FALSE);
		if (!mv_zintcmd)
			add_int_to_abs_time(&cur_time, ms, &end_time);
		else
		{
			end_time = mv_zintcmd->mv_st_cont.mvs_zintcmd.end_or_remain;
			cur_time = sub_abs_time(&end_time, &cur_time);	/* get remaing time to sleep */
			if (0 <= cur_time.at_sec)
				ms = (int4)(cur_time.at_sec * 1000 + cur_time.at_usec / 1000);
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
		UNIX_ONLY(hiber_start(ms);)
		VMS_ONLY(
			time[0] = -time_low_ms(ms);
			time[1] = -time_high_ms(ms) - 1;
			efn_mask = (1 << efn_outofband | 1 << efn_timer);
			if (SS$_NORMAL != (status = sys$setimr(efn_timer, &time, NULL, &time, 0)))
				rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("$setimr"), CALLFROM, status);
			if (SS$_NORMAL != (status = sys$wflor(efn_outofband, efn_mask)))
				rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("$wflor"), CALLFROM, status);
		)
		if (outofband)
		{
			VMS_ONLY(
				if (SS$_WASCLR == (status = sys$readef(efn_timer, &efn_mask)))
				{
					if (SS$_NORMAL != (status = sys$cantim(&time, 0)))
						rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("$cantim"), CALLFROM,
							status);
				} else
					assertpro(SS$_WASSET == status);
			)
		}
	} else
		rel_quant();
	if (outofband)
	{
		PUSH_MV_STENT(MVST_ZINTCMD);
		mv_chain->mv_st_cont.mvs_zintcmd.end_or_remain = end_time;
		mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_check = restart_ctxt;
		mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_check = restart_pc;
		/* save current information from zintcmd_active */
		mv_chain->mv_st_cont.mvs_zintcmd.restart_ctxt_prior = TAREF1(zintcmd_active, ZINTCMD_HANG).restart_ctxt_last;
		mv_chain->mv_st_cont.mvs_zintcmd.restart_pc_prior = TAREF1(zintcmd_active, ZINTCMD_HANG).restart_pc_last;
		TAREF1(zintcmd_active, ZINTCMD_HANG).restart_pc_last = restart_pc;
		TAREF1(zintcmd_active, ZINTCMD_HANG).restart_ctxt_last = restart_ctxt;
		TAREF1(zintcmd_active, ZINTCMD_HANG).count++;
		mv_chain->mv_st_cont.mvs_zintcmd.command = ZINTCMD_HANG;
		outofband_action(FALSE);
	}
	return;
}
