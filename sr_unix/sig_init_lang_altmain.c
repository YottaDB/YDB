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

#include "mdef.h"

#include "gtm_signal.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_pthread.h"

#include "io.h"
#include "gtmio.h"
#include "continue_handler.h"
#include "sig_init.h"
#include "libyottadb_int.h"
#include "generic_signal_handler.h"
#include "mdq.h"

GBLREF	struct sigaction	orig_sig_action[];
GBLREF	stack_t			oldaltstack;
GBLREF	char			*altstackptr;
GBLREF	pthread_mutex_t		sigPendingMutex;	/* Used to control access to sigPendingQue */
GBLREF	pthread_t		gtm_main_thread_id;

/* Routine to initialize an array of handlers for various signals where the main routine of this process (a non-YottaDB main
 * program - currently only Go is supported) is performing signal handling duties and routing the notification of chosen signals
 * to YottaDB. If any new signal gets handled by YottaDB, a requisite change is needed in the language wrapper using this entry
 * point to add the new signal to those it sets up forwarding to YottaDB for.
 *
 * For Go, this would mean changing initializeYottaDB() located in init.go.
 *
 * Note this routine does the same function (signal handling initialization) for alternate signal handling as sig_init() does
 * for regular signal handling. Any changes here may need to be reflected there.
 */
void sig_init_lang_altmain()
{
	int			rc, i;
	struct sigaction	sigchk_action;
	pthread_mutexattr_t	sigPendingAttr;		/* Mutex attribute we use for the signal pending queue */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGSIGHND((stderr, "sig_init_lang_altmain: Initializing alternate signal processing\n"));
	/* Define the handlers for use with simple (threaded) API. Note we do not define handlers here for SIGALRM and SIGUSR1
	 * as those handlers are initialized when needed.
	 *
	 * Note the handlers setup here with SET_ALTERNATE_SIGHANDLER() are for the same signals that sig_init() sets up for
	 * "normal" signal handling. The ydb_xxxxxx_sighandler() routines are "glue routines" that drive the same actual
	 * handlers that normal signal handling does but drives them with the correct parameters and can do something with
	 * a return code if needed.
	 */
	process_pending_signals_fptr = &process_pending_signals;	/* Setup function pointer */
	SET_ALTERNATE_SIGHANDLER(SIGABRT, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGBUS, &ydb_altmain_sighandler);
#	ifndef DISABLE_SIGCONT_PROCESSING
	if (FALSE == TREF(disable_sigcont))
		SET_ALTERNATE_SIGHANDLER(SIGCONT, &ydb_altcont_sighandler);
#	else
	TREF(disable_sigcont) = TRUE;
#	endif
	SET_ALTERNATE_SIGHANDLER(SIGFPE, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGILL, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGINT, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGIOT, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGQUIT, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGSEGV, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGTERM, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGTRAP, &ydb_altmain_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGTSTP, &ydb_altsusp_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGTTIN, &ydb_altsusp_sighandler);
	SET_ALTERNATE_SIGHANDLER(SIGTTOU, &ydb_altsusp_sighandler);
	/* We need to define a (real) signal handler for one signal as we have the need to poke the main signal thread
	 * (ydb_stm_thread) so it wakes up and looks for pending signals and tries to process them if it can get the
	 * YDB engine lock. If not, it is up to other threads to process the signal. This just uses the null_handler()
	 * handler that just returns. The purpose is just a wakeup. The signal is YDBSIGNOTIFY which is currently
	 * defined as SIGUSR2. Another potential candidate is SIGSTKFLT which is otherwise unused (obsolete).
	 */
	memset(&sigchk_action, 0, SIZEOF(sigchk_action));
	sigemptyset(&sigchk_action.sa_mask);
	sigchk_action.sa_flags = YDB_SIGACTION_FLAGS;
	sigchk_action.sa_sigaction = null_handler;
	sigaction(YDBSIGNOTIFY, &sigchk_action, &orig_sig_action[YDBSIGNOTIFY]);
	/* We need to expand the alternate stack that is used by some main languages to suit YottaDB. So if one is defined,
	 * make sure it is large enough and if not, expand it.
	 */
	setup_altstack();
	/* Need to initialize a mutex attribute that will be used to create the signal pending queue mutex */
	rc = pthread_mutexattr_init(&sigPendingAttr);
	if (0 != rc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutexattr_init()"),
			      CALLFROM, rc);
	rc = pthread_mutexattr_settype(&sigPendingAttr, PTHREAD_MUTEX_ERRORCHECK);
	if (0 != rc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutexattr_settype()"),
			      CALLFROM, rc);
	rc = pthread_mutexattr_setrobust(&sigPendingAttr, PTHREAD_MUTEX_ROBUST);
	if (0 != rc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutexattr_setrobust()"),
			      CALLFROM, rc);
	/* Now, initialize the mutex used for locking the signal pending queue */
	rc = pthread_mutex_init(&sigPendingMutex, &sigPendingAttr);
	if (0 != rc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutex_init()"), CALLFROM, rc);
}
