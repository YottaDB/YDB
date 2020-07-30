/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* This is the signal handler function that is invoked by the Operating System currently for the following signals.
 *
 * Set up in gt_timers.c
 *	SIGALRM	-> Invokes "timer_handler()"
 *
 * Set up in sig_init.c
 *	SIGINT	-> Invokes "generic_signal_handler()"
 *	SIGSEGV	-> Invokes "generic_signal_handler()"
 *	SIGABRT	-> Invokes "generic_signal_handler()"
 *	SIGBUS	-> Invokes "generic_signal_handler()"
 *	SIGFPE	-> Invokes "generic_signal_handler()"
 *	SIGIOT	-> Invokes "generic_signal_handler()"
 *	SIGILL	-> Invokes "generic_signal_handler()"
 *	SIGQUIT	-> Invokes "generic_signal_handler()"
 *	SIGTERM	-> Invokes "generic_signal_handler()"
 *	SIGTRAP	-> Invokes "generic_signal_handler()"
 *
 * The reason to have this function is to know inside "timer_handler()" and "generic_signal_handler()" whether
 * they are invoked from within an OS signal handler or not. If invoked from within an OS signal handler they
 * cannot do operations that are not async-signal safe (e.g. malloc/free etc.). This OS signal handler function
 * will pass a "boolean_t" typed parameter to those functions that indicates whether they are being invoked
 * from inside an OS signal handler or not.
 *
 * Note that this function is not invoked as the OS signal handler only for a small subset of the total list of signals.
 * This is because only "timer_handler()" and "generic_signal_handler()" are the 2 signal handler functions in YottaDB
 * that can invoke various async-signal-unsafe functions and hence need to take care. All other signal handler functions
 * (e.g. "suspsigs_handler()", "job_term_handler()", "jobinterrupt_event()", "jobexam_signal_handler()" etc.) are simplistic
 * and do not use such functions so don't yet need this extra OS signal handler step. If/when that changes, they would also
 * need to be moved into this framework.
 */

#include "mdef.h"

#include "gtm_signal.h"
#include "ydb_os_signal_handler.h"
#include "generic_signal_handler.h"
#include "gt_timer.h"

void ydb_os_signal_handler(int sig, siginfo_t *info, void *context)
{
	assert(0 <= in_os_signal_handler);
	assert(4 > in_os_signal_handler);	/* Ensure this value goes not go beyond a reasonably small number */
	/* Since at this point, in a multi-threaded program, we are not guaranteed to hold the YDB engine lock, we cannot
	 * update the "in_os_signal_handler" global variable. Therefore we do this after the FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED
	 * macro invocation in each of the below signal handler functions ("timer_handler()" and "generic_signal_handler()").
	 */
	switch(sig)
	{
	case SIGALRM:
		/* Invoke "timer_handler()" */
		timer_handler(sig, info, context, IS_OS_SIGNAL_HANDLER_TRUE);
		break;
	default:
		/* Invoke "generic_signal_handler()" */
#		ifdef DEBUG
		switch(sig)
		{
		case SIGINT:
		case SIGSEGV:
		case SIGABRT:	/* also takes care of case SIGIOT: */
		case SIGBUS:
		case SIGFPE:
		case SIGILL:
		case SIGQUIT:
		case SIGTERM:
		case SIGTRAP:
			break;
		default:
			assert(FALSE);
			break;
		}
#		endif
		generic_signal_handler(sig, info, context, IS_OS_SIGNAL_HANDLER_TRUE);
		break;
	}
	assert(0 <= in_os_signal_handler);
}

