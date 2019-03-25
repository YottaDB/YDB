/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "have_crit.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "sig_init.h"

GBLREF	int		process_exiting;
GBLREF	VSIG_ATOMIC_T	forced_exit;

void	deferred_signal_handler(void)
{
	DEBUG_ONLY(char *dummy_rname;)

	assert(DEFERRED_SIGNAL_HANDLING_TIMERS < DEFERRED_SIGNAL_HANDLING_CTRLZ);
	assert(DEFERRED_SIGNAL_HANDLING_CTRLZ < DEFERRED_SIGNAL_HANDLING_EXIT);
	assert(!INSIDE_THREADED_CODE(dummy_rname));		/* DEFERRED_SIGNAL_HANDLING_CHECK_TRIMMED ensures this */
	assert(INTRPT_OK_TO_INTERRUPT == intrpt_ok_state);	/* DEFERRED_SIGNAL_HANDLING_CHECK_TRIMMED ensures this */
	/* Even though "deferred_signal_handling_needed" was TRUE when this function was called in the
	 * DEFERRED_SIGNAL_HANDLING_CHECK_TRIMMED macro, it is possible a timer pop happens between then
	 * and here and clears the global in case it only had the DEFERRED_SIGNAL_HANDLING_TIMERS bit set.
	 * Hence we cannot assert anything about this global here.
	 */
	assert(!GET_DEFERRED_EXIT_CHECK_NEEDED || (1 == forced_exit));
	assert(GET_DEFERRED_EXIT_CHECK_NEEDED || (1 != forced_exit));
	if (process_exiting)
		return;	/* Process is already exiting. Skip handling deferred events in that case. */
	if (simpleThreadAPI_active)
	{
		if (timer_in_handler)
		{	/* Process is in a timer handler and has multiple threads. Exit handling could
			 * 	a) do "pthread_mutex_lock" calls (see PTHREAD_MUTEX_LOCK_IF_NEEDED comment in wcs_wtstart.c
			 *		for example) OR
			 *	b) do "wcs_flu" which in turn could not flush anything because "wcs_wtstart" chose not to do any
			 *		flushes (for the same reason as (a))
			 * Due to these and possibly other reasons, it is not safe to do exit handling while inside a timer handler
			 * when multiple threads are active. Therefore return right away. "deferred_signal_handler" will be invoked
			 * again at a safer point once the timer handler is done (e.g. at a later ENABLE_INTERRUPTS call)
			 */
			return;
		}
		STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED;
	}
	if (!OK_TO_INTERRUPT_TRIMMED)
		return;	/* Not in a position to allow interrupt to happen. Defer interrupt handling to later. */
	/* If forced_exit was set while in a deferred state, disregard any deferred timers or deferred Ctrl-Zs
	 * and invoke deferred_exit_handler directly (note: this can cause us to terminate/exit the process).
	 */
	if (forced_exit)
	{
		if (GET_DEFERRED_EXIT_CHECK_NEEDED)
			deferred_exit_handler();
	} else
	{
		if (GET_DEFERRED_TIMERS_CHECK_NEEDED)
			check_for_deferred_timers();
		if (GET_DEFERRED_CTRLZ_CHECK_NEEDED)
		{	/* Clear the fact that we need deferred Ctrl-Z handling before doing "suspend"
			 * as the latter can call some other function which has a deferred zone and
			 * in turn invokes DEFERRED_SIGNAL_HANDLING_CHECK* macro at which point we do
			 * not want to again do a nested suspend(SIGSTOP) processing.
			 */
			CLEAR_DEFERRED_CTRLZ_CHECK_NEEDED;
			suspend(SIGSTOP);
		}
	}
}
