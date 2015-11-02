/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Define handling of ALL signals. Most are now ignored but nothing should interrupt us
 * without our knowledge and a handler defined.
 */
#include "mdef.h"

#include "gtm_string.h"

#include <signal.h>
#include "continue_handler.h"
#include "sig_init.h"
#include "gtmci_signals.h"

void	null_handler(int sig);

void sig_init(void (*signal_handler)(), void (*ctrlc_handler)(), void (*suspsig_handler)())
{
	struct sigaction 	ignore, act;
	int			sig;
        DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;

	memset(&act, 0, SIZEOF(act));
	sigemptyset(&act.sa_mask);
	ignore = act;
	ignore.sa_handler = SIG_IGN;

	for (sig = 1; sig <= NSIG; sig++)
		sigaction(sig, &ignore, NULL);

        /* --------------------------------------------------------------
	 * Tandem hack:  rather than ignore SIGHUP, we must catch it
         * and do nothing with the signal.  This prevents ioctls on
         * modem/tty devices from hanging when carrier drops during
         * the system call.
	 * --------------------------------------------------------------
         */
	act.sa_handler = null_handler;
	sigaction(SIGHUP, &act, 0);

	/* --------------------------------------------------------------
	 * Default handling necessary for SIGCLD signal.
	 * CAUTION :consider the affect on JOB (timeout) implementation before
 	 * changing this behaviour (like defuncts, ECHILD errors etc..)
	 * --------------------------------------------------------------
	 */
	act.sa_handler = SIG_DFL;
	sigaction(SIGCLD, &act, 0);

	/* --------------------------------------------------------------
	 * Give us extra info on the following signals and a full core
	 * if necessary.
	 * --------------------------------------------------------------
	 */
	act.sa_flags = SA_SIGINFO;

	/* --------------------------------------------------------------
	 * Signals that suspend a process
	 * --------------------------------------------------------------
	 */
	if (NULL != suspsig_handler)
	{
		act.sa_sigaction = suspsig_handler;
		sigaction(SIGTSTP, &act, 0);
		sigaction(SIGTTIN, &act, 0);
		sigaction(SIGTTOU, &act, 0);
	}

	/* --------------------------------------------------------------
	 * Set special rundown handler for the following terminal signals
	 * --------------------------------------------------------------
	 */
	act.sa_sigaction = signal_handler;

	sigaction(SIGABRT, &act, 0);
	sigaction(SIGBUS, &act, 0);
#ifdef _AIX
        sigaction(SIGDANGER, &act, 0);
#endif
	sigaction(SIGFPE, &act, 0);
#ifdef __MVS__
	sigaction(SIGABND, &act, 0);
#else
#  ifndef __linux__
	sigaction(SIGEMT, &act, 0);
#  endif
#ifndef __CYGWIN__
	sigaction(SIGIOT, &act, 0);
#endif
#endif
	sigaction(SIGILL, &act, 0);
	sigaction(SIGQUIT, &act, 0);
	sigaction(SIGSEGV, &act, 0);
#ifndef __linux__
	sigaction(SIGSYS, &act, 0);
#endif
	sigaction(SIGTERM, &act, 0);
	sigaction(SIGTRAP, &act, 0);

	/* --------------------------------------------------------------
	 * If supplied with a control-C handler, install it now.
	 * --------------------------------------------------------------
	 */
	if (NULL != ctrlc_handler)
	{
		act.sa_sigaction = ctrlc_handler;
		sigaction(SIGINT, &act, 0);
	}

	/* --------------------------------------------------------------
	 * Special handling for SIGCONT
	 * --------------------------------------------------------------
	 */
#ifndef DISABLE_SIGCONT_PROCESSING
	if (FALSE == TREF(disable_sigcont))
	{
		act.sa_sigaction = continue_handler;
		sigaction(SIGCONT, &act, 0);
	}
#else
	TREF(disable_sigcont) = TRUE;
#endif
}

/* Provide null signal handler */
void	null_handler(int sig)
{
	/* */
}
