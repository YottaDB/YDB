/****************************************************************
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"

#include "generic_signal_handler.h"
#include "invocation_mode.h"

GBLREF	struct sigaction	orig_sig_action[];

/* Define a type for a sighandler_t variation for when SA_SIGINFO is not used and used. */
typedef void (*sighandler_1parm_t)(int);
typedef void (*sighandler_3parms_t)(int, siginfo_t *, void *);

/* This macro was originally added when YDB added a Go wrapper but after some rework, the Go wrapper now handles the signals
 * and lets YottaDB know about with "alternate signal handling mode". But since signal control is not likely to go away and
 * because we are adding wrappers for other languages, we are keeping this macro around and driving signals that come in
 * with it so the original signal handlers (if any were instantiated before the YDB engine was initialized) get driven when
 * a signal comes in.
 */
void drive_non_ydb_signal_handler_if_any(char *caller, int sig, siginfo_t *info, void *context, boolean_t drive_exit)
{
	sighandler_1parm_t	sighandler1;
	sighandler_3parms_t	sighandler3;
	boolean_t		is_sig_ign, is_sig_dfl;

	assert((0 < sig) && (NSIG >= sig));
	if (!(MUMPS_CALLIN & invocation_mode))
		return;
	/* Signal handler is stored in different fields based on SA_SIGINFO bit (see "man sigaction"). Extract it out. */
	if (SA_SIGINFO & orig_sig_action[sig].sa_flags)
	{
		sighandler1 = NULL;
		sighandler3 = (sighandler_3parms_t)orig_sig_action[sig].sa_sigaction;
		is_sig_ign = (SIG_IGN == (sighandler_1parm_t)sighandler3);
		is_sig_dfl = (SIG_DFL == (sighandler_1parm_t)sighandler3);
	} else
	{
		sighandler1 = (sighandler_1parm_t)orig_sig_action[sig].sa_handler;
		sighandler3 = NULL;
		is_sig_ign = (SIG_IGN == sighandler1);
		is_sig_dfl = (SIG_DFL == sighandler1);
	}
	if (is_sig_ign)
		return;	/* Main application specifically asked to ignore signal. Return without invoking any handler. */
	if (is_sig_dfl)
	{
		if (!drive_exit)
		{	/* Main application let signal be handled by default action. If default action for signal is to
			 * terminate the process, do so by invoking "_exit()". Need to not invoke "exit()" so we avoid
			 * invoking the YottaDB exit handler as otherwise it could end up in hang (e.g. nested malloc etc.,
			 * see commit message of eb7e5dd1 for more details).
			 */
			if ((SIGQUIT == sig) || (SIGTERM == sig) || (SIGINT == sig))
			{
				assert(OK_TO_INTERRUPT);	/* ensure we are not in middle of database commit etc.
								 * as otherwise we will end up in integrity errors.
								 */
				UNDERSCORE_EXIT(-sig);
			}
		}
		return;
	}
	/* At this point "sighandler1" or "sighandler3" is a pointer to a signal handling function. */
	if (drive_exit)
	{
		DBGSIGHND((stderr, "%s : Driving ydb_stm_thread_exit() prior to signal passthru\n", caller));
		if (NULL != ydb_stm_thread_exit_fnptr)
			(*ydb_stm_thread_exit_fnptr)();
	}
	DBGSIGHND((stderr, "%s : Passing signal %d through to the caller\n", caller, sig));
	/* Invoke signal handler based on whether SA_SIGINFO was specified as it means calling the handler with 3 or 1 arguments. */
	if (NULL != sighandler3)
		(*sighandler3)(sig, info, context);	/* Note - likely to NOT return */
	else
		(*sighandler1)(sig);			/* Note - likely to NOT return */
	DBGSIGHND((stderr, "%s : Returned from signal passthru for signal %d\n", caller, sig));
	return;
}

