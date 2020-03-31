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
#ifndef ALTERNATE_SIGHANDLING_H_INCLUDED
#define ALTERNATE_SIGHANDLING_H_INCLUDED

#include "libyottadb.h"

/* A word about alternate signal handling - In normal signal handling, there are times that signals need to be bounded to
 * another thread to be properly processed. This is done by the FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED() macro but this
 * is unnecessary in alternate signal handling. The FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED() macro also does a lot of the
 * deferred signal handling in regular signal handling mode but again, there is limited reason to ever defer any signal
 * notification in alternate signal handling mode. The only signals we defer in alternate signal handling mode is SIGALRM
 * which can be deferred in either mode by the SET_DEFERRED_TIMERS_CHECK_NEEDED macro in gt_timers.c
 */

/* Function pointer to an alternate signal handler */
typedef int (*ydb_alt_sighandler_fnptr_t)(int);

/* Define structure that is used to form a chain of pending signals when we are operating in an alternate signal handling
 * environment. This differs from how deferred signals are handled in the primary signal handling environment. In the former,
 * signals are detected outside of YottaDB and we are notified of them and need to wait until we have the YDB engine lock. It
 * is possible for more than one signal (of different types) to be pending though that is expected to be rare.
 */
typedef struct sig_pending_struct
{
	struct						/* Double link queue header */
	{
		struct sig_pending_struct *fl, *bl;
	} que;
	sem_t				sigHandled;	/* When signal is handled, it is posted to notify waiter */
	ydb_alt_sighandler_fnptr_t	sigHandler;	/* Signal handler to be driven when dequeued */
	int				retCode;	/* Return code from handling the signal */
	int				sigNum;		/* Which signal we're talking about */
	boolean_t			posted;		/* We've posted this signal as complete */
} sig_pending;

GBLREF	int			ydb_main_lang;
GBLREF	void 			(*process_pending_signals_fptr)(void);	/* Function pointer for process_pending_signals() */
GBLREF	sig_pending		sigPendingQue;
GBLREF	GPCallback		go_panic_callback;

/* Signal used to notify signal thread (ydb_stm_thread) that a signal is pending or to otherwise wake up the signal
 * thread so it notices it is time to shutdown. We define a null handler for since it's only job is to wakup the
 * signal thread from its polling sleep.
 */
#define YDBSIGNOTIFY SIGUSR2

/* Many times we need to know if this queue is empty or not so these macros tells us */
#define SPQUE_IS_EMPTY(ANCHORPTR, QUE)	(ANCHORPTR == (ANCHORPTR)->QUE.fl)
#define SPQUE_NOT_EMPTY(ANCHORPTR, QUE) (ANCHORPTR != (ANCHORPTR)->QUE.fl)

/* Macro to determine if we are doing regular signal handling or alternate signal handling (letting the main routine field
 * signals and forward them to us. Currently only Go uses this mode of signal handling.
 */
#define USING_ALTERNATE_SIGHANDLING (YDB_MAIN_LANG_C != ydb_main_lang)

/* Macro to drive alternate signal handling processing */
#define PROCESS_PENDING_ALTERNATE_SIGNALS						\
{											\
	if (USING_ALTERNATE_SIGHANDLING && SPQUE_NOT_EMPTY(&sigPendingQue, que))	\
	{										\
		assert(NULL != process_pending_signals_fptr);				\
		(*process_pending_signals_fptr)();					\
	}										\
}

/* Macros to set handlers into the alternate handling array that is used when the main program is doing its own signal handling
 * and letting YottaDB know what signals (if any) occur with a call to ydb_sig_dispatch().
 */
#define SET_ALTERNATE_SIGHANDLER(SIGNUM, HANDLERADDR)	\
{							\
	GBLREF void *ydb_sig_handlers[];		\
	       	    					\
	assert(USING_ALTERNATE_SIGHANDLING);		\
	ydb_sig_handlers[SIGNUM] = HANDLERADDR;		\
}

/* Macro to drive the alternate signal handling callback routine (address of which supplied at initialization) which
 * for Go, drives a routine that does a panic() based on the signal supplied. Also drives the ydb_stm_thread (signal thread)
 * shutdown routine. Note at one point this was an inline routine but such created huge #include dependency loops when
 * DEBUG_SIGNAL_HANDLING was defined we made it a macro instead.
 */
#define DRIVE_ALTSIG_CALLBACK(SIG)											\
{															\
	if (NULL != ydb_stm_thread_exit_fnptr)										\
		(*ydb_stm_thread_exit_fnptr)();										\
	if (YDB_MAIN_LANG_GO == ydb_main_lang)										\
	{	/* For Go, we need to use the panic callback so the GO main panics unwrapping the calls and driving	\
		 * the defer handlers as it unwinds.									\
		 */													\
		assert(NULL != go_panic_callback);									\
		DBGSIGHND_ONLY(fprintf(stderr, "generic_signal_handler: Driving Go callback to panic for signal %d\n",	\
				       sig); fflush(stderr));	/* Engine no longer alive so don't use it */		\
		(*go_panic_callback)(SIG);										\
		assert(FALSE);			/* Should not return */							\
	}														\
	assertpro(FALSE);			/* No other language wrappers using alternate signal handling yet */	\
}

int ydb_altalrm_sighandler(int signum);
int ydb_altcont_sighandler(int signum);
int ydb_altio_sighandler(int signum);		/* Alternate handler for SIGIO/SIGURG for GT.CM usage */
int ydb_altmain_sighandler(int signum);
int ydb_altsusp_sighandler(int signum);		/* Alternate handler for various suspend/resume signal handlers */
int ydb_altusr1_sighandler(int signum);
void process_pending_signals(void);
void sig_init_lang_altmain(void);

#endif
