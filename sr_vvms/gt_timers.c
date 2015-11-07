/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * --------------------------------------------------------------
 * Following routines are top level, user callable
 * routines of this package:
 *
 * void sys_get_cur_time(ABS_TIME atp)
 * 	fetch absolute time into stucture
 *
 * void hiber_start(uint4 hiber)
 *      used to sleep for hiber milliseconds
 *
 * void start_timer(TID tid, int4 time_to_expir, void (*handler)(), int4 dummy_hdata_len, char *dummy_hdata)
 *	Used to start a new timer.
 *
 * void cancel_timer(TID tid)
 *	Cancel an existing timer.
 *	Cancelling timer with tid = 0, cancells all timers.
 * --------------------------------------------------------------
 */
#include "mdef.h"

#include <ssdef.h>

#include "efn.h"
#include "gt_timer.h"
#include "timedef.h"
#include "wake.h"

#define MAX_INT 4294967295.0

GBLREF	int	process_exiting;

/*
 * ----------------------------------------------------
 * Get current clock time in milliseconds
 *	Fill-in the structure with the absolute time
 *	of system clock.
 *
 * Arguments:
 *	atp	- pointer to structure of absolute time
 * ----------------------------------------------------
 */
void sys_get_curr_time(ABS_TIME *atp)
{
	uint4		status, systim[2];

	status = sys$gettim(systim);
	if (status & 1)
	{
		atp->at_usec = (systim[0] / 10) % 1000000;
		atp->at_sec = (uint4)(((((double)systim[1]) * MAX_INT) + (double)systim[0]) / 10000000.0);
		return;
	}
	rts_error(VARLSTCNT(1) status);
}

static void hiber_start_ast(void)
{ /* Only purpose of this function is to provide a unique identifier for hiber_start timr driven while in an AST */
	return;
}

/*
 * ------------------------------------------------------
 * Start hibernating by starting a timer using hiber_time
 * (in msecs) and doing a pause
 * ------------------------------------------------------
 */
void hiber_start(uint4 hiber)
{
	int4 	hiber_time[2];	/* don't have static since can be interrupted by an AST */
	int	status_timr, status_wait, ast_in_prog;

	if (0 == hiber)
		return;	/* in PRO code return */

	hiber_time[0] = -time_low_ms((int4)hiber);
	hiber_time[1] = -time_high_ms((int4)hiber);
	if (hiber_time[1] == 0)
		hiber_time[1] -= 1;

	if (0 != (ast_in_prog = lib$ast_in_prog()))
	{	/* sleep sounder but less permanently;
		 * note that an AST may cause an inappropriate time to be used for another hiber_start in progress,
		 * but that risk should be statistically small, and the consequences (as far as known) are not important
		 */
		status_timr = sys$setimr(efn_timer_ast, hiber_time, 0, hiber_start_ast, 0);
		assert(SS$_NORMAL == status_timr);
		if (SS$_NORMAL == status_timr)
		{
			status_wait = sys$waitfr(efn_timer_ast);
			assert(SS$_NORMAL == status_wait);
		}
	} else
	{ /* timr->hiber should not be changed to timr->waitfr. The former waits for a wakeup or outofband event; whichever
	   * happens sooner will stop the hiber while the latter does not recognize outofband events (like tp timeouts)
	   */
		status_timr = sys$setimr(efn_hiber_start, hiber_time, wake, hiber_start, 0);
		assert(SS$_NORMAL == status_timr);
		if (SS$_NORMAL == status_timr)
		{
			sys$hiber();
			sys$cantim(hiber_start, 0);
		}
	}
}

/*
 * ----------------------------------------------------
 * System call to set timer.
 *	Time is given im msecs.
 *
 * Arguments:
 *	tid		- timer id
 *	time_to_expir	- time to expiration.
 *	handler		- address of handler routine
 * ----------------------------------------------------
 */
void start_timer(TID tid, int4 time_to_expir, void (*handler)(), int4 dummy_hdata_len, void *dummy_hdata)
{
	int4	time[2];
	int	status;

	time[1] = -time_high_ms(time_to_expir) - 1;
	time[0] = -time_low_ms(time_to_expir);
	status = sys$setimr(efn_timer, time, handler, tid, 0);
	assert(SS$_NORMAL == status);
}


/*
 * ---------------------------------------------
 * System call to cancel timer.
 * ---------------------------------------------
 */
void cancel_timer(TID tid)
{
	/* An interrupt should never cancel a timer that has been started in the mainline code.
	 * Or else it is possible the mainline code might hibernate for ever.
	 * In VMS, interrupt is equivalent to being in an AST. Hence assert we are never in an AST if we are here.
	 * The only exception is if we are exiting in which case we are not going to be hibernating so it is ok.
	 */
	assert(!lib$ast_in_prog() || process_exiting);
	sys$cantim(tid, 0);
}
