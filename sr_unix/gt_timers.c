/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2018 Stephen L Johnson.			*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* This file contains a general purpose timer package. Simultaneous multiple timers are supported.
 * All outstanding timers are contained in a queue of pending requests. New timer is added to the
 * queue in an expiration time order. The first timer in a queue expires first, and the last one
 * expires last. When the timer expires, the signal is generated and the process is awakened. This
 * timer is then removed from the queue, and the first timer in a queue is started again, and so on.
 * Starting a timer with the timer id equal to one of the existing timers in a chain will remove the
 * existing timer from the chain and add a new one instead.
 *
 * It is a responsibility of the user to go to hibernation mode by executing appropriate system call
 * if the user needs to wait for the timer expiration.
 *
 * Additionally, certain timers, designated by "safe" flag, can be processed---and, if necessary, out
 * of order---while we are deferred on interrupts. All regular timers that pop within the deferred
 * zone, will be handler in order as soon as we reenable interrupt processing.
 *
 * Following are top-level user-callable routines of this package:
 *
 * void sys_get_curr_time(ABS_TIME *atp)
 * 	fetch absolute time into stucture
 *
 * void hiber_start(uint4 hiber)
 *      used to sleep for hiber milliseconds
 *
 * void start_timer(TID tid, uint8 time_to_expir, void (*handler)(), int4 dlen, char *data)
 *	Used to start a new timer.
 *
 * void cancel_timer(TID tid)
 *	Cancel an existing timer.
 *	Cancelling timer with tid = 0, cancels all timers.
 */

#include "mdef.h"

#include "gtm_signal.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include "gtmimagename.h"
#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#ifdef YDB_USE_POSIX_TIMERS
#include <sys/syscall.h>	/* for "syscall" */
#endif

#include "gt_timer.h"
#include "wake_alarm.h"
#include "io.h"
#ifdef DEBUG
# include "wbox_test_init.h"
#endif
#ifndef __MVS__
# include <sys/param.h>
#endif
#include "send_msg.h"
#include "gtmio.h"
#include "have_crit.h"
#include "util.h"
#include "sleep.h"
#include "error.h"
#include "gtm_multi_thread.h"
#include "gtmxc_types.h"
#include "getjobnum.h"
#include "sig_init.h"
#include "libyottadb_int.h"
#include "invocation_mode.h"
#include "ydb_os_signal_handler.h"

/*#define DEBUG_SIGSAFE*/
#ifdef DEBUG_SIGSAFE
# define DBGSIGSAFEFPF(x) DBGFPF(x)
/*#include "gtmio.h"*/
#include "io.h"
#else
# define DBGSIGSAFEFPF(x)
#endif

#ifdef ITIMER_REAL
# define USER_HZ 1000
#endif

#define TIMER_BLOCK_SIZE	64	/* # of timer entries allocated initially as well as at every expansion */
#define GT_TIMER_EXPAND_TRIGGER	8	/* if the # of timer entries in the free queue goes below this, allocate more */
#define MAX_TIMER_POP_TRACE_SZ	32

#define ADD_SAFE_HNDLR(HNDLR)									\
{												\
	assert((ARRAYSIZE(safe_handlers) - 1) > safe_handlers_cnt);				\
	assert(NULL != (void *)HNDLR); /* void * to avoid warnings of always true */		\
	safe_handlers[safe_handlers_cnt++] = HNDLR;						\
}

#ifdef YDB_USE_POSIX_TIMERS
	STATICDEF struct itimerspec	sys_timer, old_sys_timer;
	STATICDEF ABS_TIME		sys_timer_at;			/* Absolute time associated with sys_timer */
#else
#	define REPORT_SETITIMER_ERROR(TIMER_TYPE, SYS_TIMER, FATAL, ERRNO)				\
	MBSTART {											\
		char s[512];										\
													\
		SNPRINTF(s, 512, "Timer: %s; timer_active: %d; "					\
			"sys_timer.it_value: [tv_sec: %ld; tv_usec: %ld]; "				\
			"sys_timer.it_interval: [tv_sec: %ld; tv_usec: %ld]",				\
			TIMER_TYPE, timer_active,							\
			SYS_TIMER.it_value.tv_sec, SYS_TIMER.it_value.tv_usec,				\
			SYS_TIMER.it_interval.tv_sec, SYS_TIMER.it_interval.tv_usec);			\
		if (FATAL)										\
		{											\
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7)						\
				ERR_SETITIMERFAILED, 1, ERRNO, ERR_TEXT, 2, LEN_AND_STR(s));		\
			in_setitimer_error = TRUE;							\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SETITIMERFAILED, 1, ERRNO);	\
		} else											\
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7)	MAKE_MSG_WARNING(ERR_SETITIMERFAILED),	\
				1, ERRNO, ERR_TEXT, 2, LEN_AND_STR(s));					\
	} MBEND
	STATICDEF struct itimerval	sys_timer, old_sys_timer;
	STATICDEF ABS_TIME		sys_timer_at;			/* Absolute time associated with sys_timer */
	STATICDEF boolean_t		in_setitimer_error;
#endif

#define SAFE_FOR_ANY_TIMER	((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (FALSE == process_exiting) && !fast_lock_count)
/* In case threads are running, we don't want any unsafe timers to be handled during a timer handler pop. This is because we
 * don't know if the threads will modify the same global variable that the unsafe timer modifies concurrently.
 * But it is okay for timers to be started by individual threads. For example the iott_flush_timer will be started inside
 * thread code only while holding a mutex lock (e.g. inside gtm_putmsg_list or so) and even though a "setitimer" call is done
 * inside one thread, the SIGALRM pop will happen only in the parent process because all threads have SIGALRM disabled in their
 * signal mask. Define SAFE_FOR_TIMER_POP and SAFE_FOR_TIMER_START variables accordingly.
 */
#define	SAFE_FOR_TIMER_POP	(SAFE_FOR_ANY_TIMER && !multi_thread_in_use)
#define	SAFE_FOR_TIMER_START	(SAFE_FOR_ANY_TIMER)

/* The below timer handler functions all invoke functions that are not listed as async-signal-safe (see "man signal-safety")
 * and so cannot be invoked from inside an os signal handler. Instead they have to be invoked after the signal handler returns
 * in a deferred fashion.
 */
#define	IS_KNOWN_UNSAFE_TIMER_HANDLER(HNDLR)									\
	((wcs_stale_fptr == HNDLR)		/* wcs_wtstart() -> send_msg_csa() -> syslog() [UNSAFE] */	\
	 || (wcs_clean_dbsync_fptr == HNDLR)	/* wcs_clean_dbsync() -> send_msg_csa() -> syslog() [UNSAFE] */	\
	 || (jnl_file_close_timer_fptr == HNDLR))	/* jnl_file_close_timer() -> shmdt() [UNSAFE] */

/* Macros to set the global timer_from_OS - is set when timer_handler() is entered as an OS call instead of as a dummy
 * deferred interrupt type call. Cleared when timer_handler exits.
 */
#define SET_TIMER_FROM_OS							\
MBSTART {									\
	assert(!timer_from_OS);			/* Shouldn't be on already */	\
	timer_from_OS = TRUE;							\
} MBEND

#define CLEAR_TIMER_FROM_OS_IF_NEEDED(IS_OS_SIG_HANDLER)					\
MBSTART {											\
	if (IS_OS_SIG_HANDLER)									\
	{											\
		assert(timer_from_OS);		/* Should be on if going to clear */		\
		timer_from_OS = FALSE;								\
	}											\
} MBEND

STATICDEF volatile GT_TIMER *timeroot = NULL;	/* chain of pending timer requests in time order */
STATICDEF boolean_t first_timeset = TRUE;
STATICDEF struct sigaction prev_alrm_handler;	/* save previous SIGALRM handler, if any */

/* Chain of unused timer request blocks */
STATICDEF volatile	GT_TIMER	*timefree = NULL;
STATICDEF volatile 	int4		num_timers_free;		/* # of timers in the unused queue */
STATICDEF volatile 	st_timer_alloc	*timer_allocs = NULL;

STATICDEF int 		safe_timer_cnt, timer_pop_cnt;			/* Number of safe timers in queue/popped */
STATICDEF TID 		*deferred_tids;

STATICDEF timer_hndlr	safe_handlers[MAX_SAFE_TIMER_HNDLRS + 1];	/* +1 for NULL to terminate list */
STATICDEF int		safe_handlers_cnt;

STATICDEF boolean_t	stolen_timer = FALSE;	/* only complain once, used in check_for_timer_pops() */
STATICDEF boolean_t	stopped_timer = FALSE;	/* only complain once, used in check_for_timer_pops() */
STATICDEF char 		*whenstolen[] = {"check_for_timer_pops", "check_for_timer_pops first time"}; /* for check_for_timer_pops */

#ifdef YDB_USE_POSIX_TIMERS
STATICDEF timer_t	posix_timer_id;
#endif

#ifdef DEBUG
STATICDEF int		trc_timerpop_idx;
STATICDEF GT_TIMER	trc_timerpop_array[MAX_TIMER_POP_TRACE_SZ];

# define TRACE_TIMER_POP(TIMER_INFO)							\
{											\
	memcpy(&trc_timerpop_array[trc_timerpop_idx], TIMER_INFO, SIZEOF(GT_TIMER));	\
	trc_timerpop_idx = (trc_timerpop_idx + 1) % MAX_TIMER_POP_TRACE_SZ;		\
}
#endif

/* Get current clock time from the target clock when using clock_gettime()
 * Note: This function is called from timer_handler and so needs to be async-signal safe.
 *       POSIX defines "clock_gettime" as safe but not "gettimeofday" so don't use the latter.
 * Arguments:	atp - pointer to absolute structure of time
 * 		clockid - one of CLOCK_REALTIME or CLOCK_MONOTONIC
 */
#define	GET_TIME_CORE(ATP, CLOCKID)									\
MBSTART {												\
	/* Note: This code is called from timer_handler and so needs to be async-signal safe.		\
	 * POSIX defines "clock_gettime" as safe but not "gettimeofday" so dont use the latter.		\
	 */												\
	clock_gettime(CLOCKID, ATP);									\
} MBEND

/* Sleep for MS milliseconds of "clockid" time unless interrupted by RESTART processing */
#ifdef _AIX
/* Because of unreliability, AIX uses plain old nanosleep() */
#define HIBER_START_CORE(MS, CLOCKID, RESTART)	SLEEP_USEC((MS * 1000UL), RESTART)
#else
#define HIBER_START_CORE(MS, CLOCKID, RESTART)						\
MBSTART {										\
	time_t	seconds, nanoseconds;							\
											\
	seconds = MS / 1000;								\
	nanoseconds = (MS % 1000) * E_6;						\
	CLOCK_NANOSLEEP(CLOCKID, seconds, nanoseconds, (RESTART), MT_SAFE_TRUE);	\
} MBEND
#endif

/* Flag signifying timer is active. Especially useful when the timer handlers get nested. This has not been moved to a
 * threaded framework because we do not know how timers will be used with threads.
 */
GBLDEF	volatile boolean_t	timer_active;
GBLDEF	volatile int4		timer_stack_count;		/* Records that we are already in timer processing */
GBLDEF	volatile boolean_t	timer_in_handler;

/* Below function pointers (indirect references to functions) exist to avoid bloating the "gtmsecshr" executable */
GBLDEF	void			(*wcs_stale_fptr)();		/* Reference to wcs_stale() to be used in gt_timers.c */
GBLDEF	void			(*wcs_clean_dbsync_fptr)();	/* Reference to wcs_clean_dbsync() to be used in gt_timers.c */
GBLDEF	void			(*jnl_file_close_timer_fptr)();	/* Reference to jnl_file_close_timer() to be used in gt_timers.c */

/* The timer_from_OS global indicates a SIGALRM from OS has occurred blocking further SIGALRMs. Note this is a boolean instead
 * of a counter as once we come in via SIGALRM, other SIGALRM interrupts are blocked. Note this var differs from the global
 * in_os_signal_handler (managed by the INCREMENT_IN_OS_SIGNAL_HANDLER/DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED macros) in that
 * it is only set for SIGALRM signals and indicates SIGALRMs are blocked whereas in_os_signal_handler is incremented for other
 * signals too so cannot provide the same information.
 */
GBLDEF	volatile boolean_t	timer_from_OS;

GBLREF	boolean_t		blocksig_initialized;		/* Set to TRUE when blockalrm, block_ttinout, and block_sigsent are
								 * initialized. */
GBLREF	boolean_t		mu_reorg_process, oldjnlclose_started;
GBLREF	int			process_exiting;
GBLREF	int4			error_condition;
GBLREF	sigset_t		blockalrm;
GBLREF	sigset_t		block_sigsent;
GBLREF	sigset_t		block_ttinout;
GBLREF	sigset_t		block_worker;
GBLREF	void			(*jnl_file_close_timer_ptr)(void);	/* Initialized only in gtm_startup(). */
GBLREF	volatile int4		fast_lock_count, outofband;
GBLREF	struct sigaction	orig_sig_action[];
GBLREF	int			ydb_main_lang;
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		simpleThreadAPI_active;
GBLREF	boolean_t		noThreadAPI_active;
#ifdef YDB_USE_POSIX_TIMERS
GBLREF	pid_t			posix_timer_thread_id;
GBLREF	boolean_t		posix_timer_created;
#endif
#ifdef DEBUG
GBLREF	boolean_t		in_nondeferrable_signal_handler;
GBLREF	boolean_t		gtm_jvm_process;
#endif

error_def(ERR_SETITIMERFAILED);
error_def(ERR_TEXT);
error_def(ERR_TIMERHANDLER);

STATICFNDCL void	gt_timers_alloc(void);
STATICFNDCL void	start_timer_int(TID tid, uint8 time_to_expir, void (*handler)(), int4 hdata_len,
					void *hdata, boolean_t safe_timer);
STATICFNDCL void	sys_settimer (TID tid, ABS_TIME *time_to_expir);
STATICFNDCL void	start_first_timer(ABS_TIME *curr_time, boolean_t is_os_signal_handler);
STATICFNDCL GT_TIMER	*find_timer(TID tid, GT_TIMER **tprev);
STATICFNDCL GT_TIMER	*add_timer(ABS_TIME *atp, TID tid, uint8 time_to_expir, void (*handler)(), int4 hdata_len,
				   void *hdata, boolean_t safe_timer);
STATICFNDCL void	remove_timer(TID tid);

/* Preallocate some memory for timers. */
void gt_timers_alloc(void)
{
	int4		gt_timer_cnt;
	GT_TIMER	*timeblk, *timeblks;
	st_timer_alloc	*new_alloc;

	/* Allocate timer blocks putting each timer on the free queue */
	assert(1 > timer_stack_count);
#	ifdef DEBUG
	/* Allocate space for deferred timer tracking. We don't expect to need more than the initial TIMER_BLOCK_SIZE entries */
	if (WBTEST_ENABLED(WBTEST_DEFERRED_TIMERS))
	{
		deferred_tids = (TID *)malloc(sizeof(TID) * TIMER_BLOCK_SIZE);
		memset((char *)deferred_tids, (char)0xff, sizeof(TID) * TIMER_BLOCK_SIZE);
	}
#	endif
	timeblk = timeblks = (GT_TIMER *)malloc((SIZEOF(GT_TIMER)) * TIMER_BLOCK_SIZE);
	new_alloc = (st_timer_alloc *)malloc(SIZEOF(st_timer_alloc));
	new_alloc->addr = timeblk;
	new_alloc->next = (st_timer_alloc *)timer_allocs;
	timer_allocs = new_alloc;
	for (gt_timer_cnt = TIMER_BLOCK_SIZE; 0 < gt_timer_cnt; --gt_timer_cnt)
	{
		timeblk->hd_len_max = GT_TIMER_INIT_DATA_LEN;	/* Set amount it can store */
		timeblk->next = (GT_TIMER *)timefree;		/* Put on free queue */
		timefree = timeblk;
		timeblk = (GT_TIMER *)((char *)timeblk + SIZEOF(GT_TIMER));	/* Next! */
	}
	assert(((char *)timeblk - (char *)timeblks) == (SIZEOF(GT_TIMER)) * TIMER_BLOCK_SIZE);
	num_timers_free += TIMER_BLOCK_SIZE;
}

void add_safe_timer_handler(int safetmr_cnt, ...)
{
	int		i;
	va_list		var;
	timer_hndlr	tmrhndlr;

	VAR_START(var, safetmr_cnt);
	for (i = 1; i <= safetmr_cnt; i++)
	{
		tmrhndlr = va_arg(var, timer_hndlr);
		ADD_SAFE_HNDLR(tmrhndlr);
	}
	va_end(var);
}

/* Do the initialization of blockalrm, block_ttinout and block_sigsent, and set blocksig_initialized to TRUE, so
 * that we can later block signals when there is a need. This function should be called very early
 * in the main() routines of modules that wish to do their own interrupt handling.
 */
void set_blocksig(void)
{
	sigemptyset(&blockalrm);
	sigaddset(&blockalrm, SIGALRM);
	sigemptyset(&block_ttinout);
	sigaddset(&block_ttinout, SIGTTIN);
	sigaddset(&block_ttinout, SIGTTOU);
	sigemptyset(&block_sigsent);
	sigaddset(&block_sigsent, SIGINT);
	sigaddset(&block_sigsent, SIGQUIT);
	sigaddset(&block_sigsent, SIGTERM);
	sigaddset(&block_sigsent, SIGTSTP);
	sigaddset(&block_sigsent, SIGCONT);
	sigaddset(&block_sigsent, SIGALRM);
	assert(YDBSIGNOTIFY == SIGUSR2);		/* Just recording which signal this actually is */
	sigaddset(&block_sigsent, YDBSIGNOTIFY);	/* Used in alternate signal processing */
	sigfillset(&block_worker);
	sigdelset(&block_worker, SIGSEGV);
	sigdelset(&block_worker, SIGKILL);
	sigdelset(&block_worker, SIGFPE);
	sigdelset(&block_worker, SIGBUS);
	sigaddset(&block_worker, SIGTERM);
	blocksig_initialized = TRUE;	/* note the fact that blockalrm and block_sigsent are initialized */
}

/* Initialize group of timer blocks */
void prealloc_gt_timers(void)
{	/* Preallocate some timer blocks. This will be all the timer blocks we hope to need.
	 * Allocate them with 8 bytes of possible data each.
	 * If more timer blocks are needed, we will allocate them as needed.
	 */
	gt_timers_alloc();			/* No preventing SIGALRMS here this early in initialization */
	/* Now initialize the safe timers. Must be done dynamically to avoid the situation where this module always references all
	 * possible safe timers thus pulling extra stuff into executables that don't need or want it.
	 *
	 * First step, fill in the safe timers contained within this module which are always available.
	 * NOTE: Use gt_timers_add_safe_hndlrs to add safe timer handlers not visible to gtmsecshr
	 */
	ADD_SAFE_HNDLR(&wake_alarm);		/* Standalone module containing only one global reference */
}

/* Get current monotonic clock time. Fill-in the structure with the monotonic time of system clock */
void sys_get_curr_time(ABS_TIME *atp)
{
	/* Note: CLOCK_MONOTONIC comes more recommended than CLOCK_REALTIME since it is immune to adjustments
	 *       from NTP and adjtime(). So we use that.
	 */
	GET_TIME_CORE(atp, CLOCK_MONOTONIC);
}

/* Get current "wall" clock time. Fill-in the structure with the absolute time of system clock.
 * WARNING: only op_hang uses this to ensure the HANG duration matches $[Z]Horlog/$ZUT time
 */
void sys_get_wall_time(ABS_TIME *atp)
{
	GET_TIME_CORE(atp, CLOCK_REALTIME);
}

/* Sleep for milliseconds of monotonic time unless interrupted by out-of-band processing */
void hiber_start(uint4 milliseconds)
{
	/* WARNING: negation of outofband is intentional */
	HIBER_START_CORE(milliseconds, CLOCK_MONOTONIC, !outofband);
}

/* Sleep for milliseconds of "wall clock" time unless interrupted by out-of-band processing
 * WARNING: only op_hang uses this to ensure the HANG duration matches $[Z]Horlog/$ZUT time
 */
void hiber_start_wall_time(uint4 milliseconds)
{
	/* WARNING: negation of outofband is intentional */
	HIBER_START_CORE(milliseconds, CLOCK_REALTIME, !outofband);
}

/* Sleep for milliseconds of time unless EINTRrupted */
void hiber_start_wait_any(uint4 milliseconds)
{
	HIBER_START_CORE(milliseconds, CLOCK_MONOTONIC, FALSE);	/* FALSE passed so we return if an EINTR occurs
								 * instead of restarting wait.
								 */
}

/* Wrapper function for start_timer() that is exposed for outside use. The function ensures that time_to_expir is positive. If
 * negative value or 0 is passed, set time_to_expir to 0 and invoke start_timer(). The reason we have not merged this functionality
 * with start_timer() is because there is no easy way to determine whether the function is invoked from inside YottaDB or by an
 * external routine.
 * Arguments:	tid 		- timer id
 *		time_to_expir	- time to expiration in nanosecs
 *		handler		- pointer to handler routine
 *		hdata_len       - length of handler data next arg
 *		hdata		- data to pass to handler (if any)
 */
void gtm_start_timer(TID tid,
		 int4 time_to_expir,
		 void (*handler)(),
		 int4 hdata_len,
		 void *hdata)
{
	if (0 >= time_to_expir)
		time_to_expir = 0;
	start_timer(tid, time_to_expir * (uint8)NANOSECS_IN_MSEC, handler, hdata_len, hdata);
}

/* Start the timer. If timer chain is empty or this is the first timer to expire, actually start the system timer.
 * Arguments:	tid 		- timer id
 *		time_to_expir	- time to expiration in nanosecs
 *		handler		- pointer to handler routine
 *      	hdata_len       - length of handler data next arg
 *      	hdata		- data to pass to handler (if any)
 */
void start_timer(TID tid, uint8 time_to_expir, void (*handler)(), int4 hdata_len, void *hdata)
{
	sigset_t		savemask;
	boolean_t		safe_timer = FALSE, safe_to_add = FALSE;
	int			i, rc;
#ifdef DEBUG
	struct itimerval	curtimer;
#endif

	if (exit_handler_active)
	{	/* Starting a timer while we have already started exiting can cause issues in an environment where
		 * the main program is not YottaDB (e.g. Go program etc.). This is because YottaDB cancels its timer
		 * at the start of exiting and so any timers started from then on will not be canceled when exit
		 * processing is complete. These timers if they pop would land in handlers established by the main
		 * program which would not be ready to handle them. So best would be to not start such timers.
		 * We don't expect any such code paths since all timers that can potentially be started during exit
		 * handling already have checks to not invoke "start_timer()" in that case. Hence the assert below.
		 * In Release builds though, play it safe and just return.
		 */
		assert(FALSE);
		return;
	}
	assertpro(0 <= time_to_expir);			/* Callers should verify non-zero time */
	DUMP_TIMER_INFO("At the start of start_timer()");
	if (NULL == handler)
	{
		safe_to_add = TRUE;
		safe_timer = TRUE;
	} else if (wcs_clean_dbsync_fptr == handler)
	{	/* Account for known instances of the above function being called from within a deferred zone. */
		assert((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) || (INTRPT_IN_WCS_WTSTART == intrpt_ok_state)
			|| (INTRPT_IN_GDS_RUNDOWN == intrpt_ok_state) || (INTRPT_IN_DB_CSH_GETN == intrpt_ok_state)
			|| (mu_reorg_process && (INTRPT_IN_KILL_CLEANUP == intrpt_ok_state)));
		safe_to_add = TRUE;
	} else if (wcs_stale_fptr == handler)
	{	/* Account for known instances of the above function being called from within a deferred zone. */
		assert((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) || (INTRPT_IN_DB_CSH_GETN == intrpt_ok_state)
			|| (INTRPT_IN_TRIGGER_NOMANS_LAND == intrpt_ok_state)
			|| (mu_reorg_process && (INTRPT_IN_KILL_CLEANUP == intrpt_ok_state)));
		safe_to_add = TRUE;
	} else if (jnl_file_close_timer_ptr == handler)
	{	/* Account for known instances of the above function being called from within a deferred zone. */
		assert((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) || (INTRPT_IN_DB_CSH_GETN == intrpt_ok_state)
			|| (INTRPT_IN_TRIGGER_NOMANS_LAND == intrpt_ok_state) || (INTRPT_IN_SS_INITIATE == intrpt_ok_state)
			|| (INTRPT_IN_GDS_RUNDOWN == intrpt_ok_state)
			|| (mu_reorg_process && (INTRPT_IN_KILL_CLEANUP == intrpt_ok_state)));
		safe_to_add = TRUE;
	} else
	{
		for (i = 0; NULL != safe_handlers[i]; i++)
		{
			if (safe_handlers[i] == handler)
			{
				safe_to_add = TRUE;
				safe_timer = TRUE;
				break;
			}
		}
	}
	if (!safe_to_add && !SAFE_FOR_TIMER_START)
	{
		assert(FALSE);
		return;
	}
	if (!timer_from_OS)
	{
		SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);	/* block SIGALRM signal */
#		ifdef DEBUG
		if (TRUE == timer_active)	/* There had better be an active timer */
			assert(0 == getitimer(ITIMER_REAL, &curtimer));
#		endif
	}
	start_timer_int(tid, time_to_expir, handler, hdata_len, hdata, safe_timer);
	if (!timer_from_OS)
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);		/* reset signal handlers */
	DUMP_TIMER_INFO("At the end of start_timer()");
}

/* Internal version of start_timer that does not protect itself, assuming this has already been done.
 * Otherwise does as explained above in start_timer.
 */
STATICFNDEF void start_timer_int(TID tid, uint8 time_to_expir, void (*handler)(), int4 hdata_len, void *hdata, boolean_t safe_timer)
{
	ABS_TIME	at;
	GT_TIMER 	*newt;

	assert(0 <= time_to_expir);
	/* there is abuse of this api - hdata is a pointer, so hd_len, if supplied, should be the size of a pointer, but callers
	 * sometimes use the size of the pointed-to data
	 */
	sys_get_curr_time(&at);
	if (first_timeset)
	{
		init_timers();
		assert(!first_timeset);		/* Verify init_timers() unset this */
	}
	/* We expect no timer with id=<tid> to exist in the timer queue currently. This is asserted in "add_timer" call below.
	 * In pro though, we'll be safe and remove any tids that exist before adding a new entry with the same tid - 2009/10.
	 * If a few years pass without the assert failing, it might be safe then to remove the PRO_ONLY code below.
	 */
#	ifndef DEBUG
	remove_timer(tid); /* Remove timer from chain */
#	endif
	/* Check if # of free timer slots is less than minimum threshold. If so, allocate more of those while it is safe to do so */
	if ((GT_TIMER_EXPAND_TRIGGER > num_timers_free) && (1 > timer_stack_count))
		gt_timers_alloc();
	DUMP_TIMER_INFO("Before invoking add_timer()");
	newt = add_timer(&at, tid, time_to_expir, handler, hdata_len, hdata, safe_timer);	/* Put new timer in the queue. */
	DUMP_TIMER_INFO("After invoking add_timer()");
	if ((timeroot->tid == tid) || !timer_active
	    || ((newt->expir_time.tv_sec < sys_timer_at.tv_sec)
		|| ((newt->expir_time.tv_sec == sys_timer_at.tv_sec)
		    && (newt->expir_time.tv_nsec < sys_timer_at.tv_nsec))))
		start_first_timer(&at, IS_OS_SIGNAL_HANDLER_FALSE);
}

/* Cancel timer.
 * Arguments:	tid - timer id
 */
void cancel_timer(TID tid)
{
	ABS_TIME	at;
	sigset_t	savemask;
	boolean_t	first_timer;
	int		rc;

	if (!timer_from_OS)
		SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);	/* block SIGALRM signal */
	DUMP_TIMER_INFO("At the start of cancel_timer()");
	sys_get_curr_time(&at);
	first_timer = (timeroot && (timeroot->tid == tid));
	remove_timer(tid);		/* remove it from the chain */
	if (first_timer)
	{
		if (timeroot)
			start_first_timer(&at, IS_OS_SIGNAL_HANDLER_FALSE);	/* start the first timer in the chain */
		else if (timer_active)
			sys_canc_timer();
	}
	if (!timer_from_OS)
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
	DUMP_TIMER_INFO("At the end of cancel_timer()");
}

/* Clear the timers' state for the forked-off process. */
void clear_timers(void)
{
	sigset_t	savemask;
	int		rc;

	DUMP_TIMER_INFO("At the start of clear_timers()");
	if (!timer_from_OS)
		SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);	/* block SIGALRM signal */
	if (NULL == timeroot)
	{	/* If no timers have been initialized in this process, take fast path (avoid system call) */
		/* If the only timer popped, and we got a SIGTERM while its handler was active, the timeroot
		 * would be NULL and timer_in_handler would be TRUE, but that should be safe for the fast path,
		 * so allow this case if the process is exiting.
		 */
		assert((FALSE == timer_in_handler) || process_exiting);
		assert(FALSE == timer_active);
		/* Note: "oldjnlclose_started" could be TRUE in case a timer with TID=jnl_file_close_timer
		 * was still active when the process decided to exit and cancel all unsafe
		 * timers (CANCEL_TIMERS call in LOCK_RUNDOWN_MACRO in "gtm_exit_handler.c").
		 * So clear this global to get it back in sync with "timeroot".
		 */
		oldjnlclose_started = FALSE;
		assert(!GET_DEFERRED_TIMERS_CHECK_NEEDED);
		if (!timer_from_OS)
			SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
		return;
	}
	while (timeroot)
		remove_timer(timeroot->tid);
	timer_in_handler = FALSE;
	timer_active = FALSE;
	oldjnlclose_started = FALSE;
	CLEAR_DEFERRED_TIMERS_CHECK_NEEDED;
	if (!timer_from_OS)
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
	DUMP_TIMER_INFO("After invoking clear_timers()");
	return;
}

/* System call to set timer. Time is given im msecs.
 * Arguments:	tid		- timer id
 *		time_to_expir	- time to expiration
 */
STATICFNDEF void sys_settimer(TID tid, ABS_TIME *time_to_expir)
{
	struct sigevent	sevp;
	int		save_errno;

#	ifdef YDB_USE_POSIX_TIMERS
	if (!posix_timer_created)
	{	/* No posix timer has yet been created */
		memset(&sevp, 0, sizeof(sevp));	/* Initialize parm block to eliminate garbage then fill in what we want */
		/* When using alternate signal handling, Go is handling the signals so there is no requirement that SIGALRM be
		 * sent to the "signal thread" (ydb_stm_thread) and in fact, that can result in a slowdown since if the signal
		 * was instead sent to a Go thread, it wouldn't have to make an expensive runtime change to process it.
		 * Note though that even if Go is the main program, if YottaDB has started exit handling, then it is better
		 * to start timers and send it to the current thread running YottaDB exit handler code (not to a Go thread)
		 * as Go cannot call back into YottaDB once exit handling has started. Hence the "exit_handler_active" check below.
		 */
		if (USING_ALTERNATE_SIGHANDLING && !exit_handler_active)
			sevp.sigev_notify = SIGEV_SIGNAL;
		else
		{
			sevp.sigev_notify = SIGEV_THREAD_ID;
			/* Determine thread id to use for notification but before that initialize "posix_timer_thread_id" */
			/* If "posix_timer_thread_id" is 0, then initialize it from the current thread id.
			 * If it is non-zero, then use that already initialized thread id. The only exception is if this is a
			 * SimpleThreadAPI environment and if "exit_handler_active" is TRUE. In that case, the MAIN worker thread
			 * id that "posix_timer_thread_id" points to is in the process of terminating and so we cannot ask for that
			 * thread to receive SIGALRM for timers started now (race condition can lead to errors about invalid timer
			 * id in "timer_create()" call etc. Therefore, use the current thread id in that case. Note that it is okay
			 * to do so even in a SimpleThreadAPI environment since the fact that the current thread reached here
			 * implies it holds the YottaDB multi-thread engine lock currently and will do so until the end of exit
			 * processing.
			 */
			if ((0 == posix_timer_thread_id) || (simpleThreadAPI_active && exit_handler_active))
				posix_timer_thread_id = syscall(SYS_gettid);
			/* Note: The man pages of "timer_create" indicate we need to fill "sevp.sigev_notify_thread_id" with the
			 * thread id but that field does not seem to be defined at least in all linux versions we currently have
			 * with the current compilation flags we use so we define it explicitly to "sevp._sigev_un._tid" which
			 * then works as desired.
			 */
#			ifndef sigev_notify_thread_id
#			define sigev_notify_thread_id _sigev_un._tid
#			endif
			sevp.sigev_notify_thread_id = posix_timer_thread_id;
		}
		sevp.sigev_signo = SIGALRM;
		if (timer_create(CLOCK_MONOTONIC, &sevp, &posix_timer_id))
		{
			save_errno = errno;
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7)
				      ERR_SYSCALL, 5, RTS_ERROR_LITERAL("timer_create()"), CALLFROM, save_errno);
		}
		posix_timer_created = TRUE;
	}
	assert(0 <= time_to_expir->tv_sec);
	assert(0 <= time_to_expir->tv_nsec);
	assert(NANOSECS_IN_SEC > time_to_expir->tv_nsec);
	sys_timer.it_value.tv_sec = time_to_expir->tv_sec;
	sys_timer.it_value.tv_nsec = time_to_expir->tv_nsec;
	assert(sys_timer.it_value.tv_sec || sys_timer.it_value.tv_nsec);
	sys_timer.it_interval.tv_sec = sys_timer.it_interval.tv_nsec = 0;
	if ((-1 == timer_settime(posix_timer_id, 0, &sys_timer, &old_sys_timer)) || WBTEST_ENABLED(WBTEST_SETITIMER_ERROR))
	{
		save_errno = errno;
		assert(WBTEST_ENABLED(WBTEST_SETITIMER_ERROR));
		WBTEST_ONLY(WBTEST_SETITIMER_ERROR,
			save_errno = EINVAL;
		);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
					ERR_SYSCALL, 5, RTS_ERROR_LITERAL("timer_settime()"), CALLFROM, save_errno);
	}
#	else
	if (in_setitimer_error)
		return;
	sys_timer.it_value.tv_sec = time_to_expir->tv_sec;
	sys_timer.it_value.tv_usec = (gtm_tv_usec_t)(time_to_expir->tv_nsec / NANOSECS_IN_USEC);
	assert(MICROSECS_IN_SEC > sys_timer.it_value.tv_usec);
	if (!sys_timer.it_value.tv_sec && !sys_timer.it_value.tv_usec)
	{	/* Case where the requested time_to_expir is < 1000 nanoseconds i.e. == 0 microsecond.
		 * We cannot pass 0 microseconds to "setitimer" as it will cause the system interval timer to stop.
		 * And we cannot treat the time_to_expir as 0 (i.e. treat the timer event as already expired) either
		 * as that would mean premature return (relative to the requested time) even if it is premature
		 * by < 1000 nanoseconds. Therefore treat this case as == 1000 nanoseconds (1 microsecond) as that
		 * is the minimum resolution of the "setitimer" call.
		 */
		sys_timer.it_value.tv_usec = 1;
	}
	sys_timer.it_interval.tv_sec = sys_timer.it_interval.tv_usec = 0;
	if ((-1 == setitimer(ITIMER_REAL, &sys_timer, &old_sys_timer)) || WBTEST_ENABLED(WBTEST_SETITIMER_ERROR))
	{
		REPORT_SETITIMER_ERROR("ITIMER_REAL", sys_timer, TRUE, errno);
	}
#		ifdef TIMER_DEBUGGING
		FPRINTF(stderr, "------------------------------------------------------\n"
			"SETITIMER\n---------------------------------\n");
		FPRINTF(stderr, "System timer :\n  expir_time: [at_sec: %ld; at_usec: %ld]\n",
			sys_timer.it_value.tv_sec, sys_timer.it_value.tv_usec);
		FPRINTF(stderr, "Old System timer :\n  expir_time: [at_sec: %ld; at_usec: %ld]\n",
			old_sys_timer.it_value.tv_sec, old_sys_timer.it_value.tv_usec);
		FFLUSH(stderr);
#		endif	/* #ifdef TIMER_DEBUGGING */
#	endif	/* #ifdef YDB_USE_POSIX_TIMERS */
	timer_active = TRUE;
}

/* Start the first timer in the timer chain
 * Arguments:	curr_time	- current time assumed within the function
 */
STATICFNDEF void start_first_timer(ABS_TIME *curr_time, boolean_t is_os_signal_handler)
{
	ABS_TIME	deltatime;
	GT_TIMER	*tpop;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DUMP_TIMER_INFO("At the start of start_first_timer()");
	CLEAR_DEFERRED_TIMERS_CHECK_NEEDED;
	if ((1 < timer_stack_count) || (TRUE == timer_in_handler))
		return;
	for (tpop = (GT_TIMER *)timeroot ; tpop ; tpop = tpop->next)
	{
		deltatime = sub_abs_time((ABS_TIME *)&tpop->expir_time, curr_time);
		if ((0 > deltatime.tv_sec) || ((0 == deltatime.tv_sec) && (0 == deltatime.tv_nsec)))
		{	/* Timer has expired. Handle safe timers, defer unsafe timers. */
			if (tpop->safe || (SAFE_FOR_TIMER_START && (1 > timer_stack_count)
						&& !(TREF(in_ext_call) && IS_KNOWN_UNSAFE_TIMER_HANDLER(tpop->handler))))
			{
				DEBUG_ONLY(STAPI_FAKE_TIMER_HANDLER_WAS_DEFERRED);
				timer_handler(DUMMY_SIG_NUM, NULL, NULL, is_os_signal_handler);
				/* At this point all timers should have been handled, including a recursive call to
				 * start_first_timer(), if needed, and SET_DEFERRED_TIMERS_CHECK_NEEDED invoked if
				 * appropriate, so we are done.
				 */
				break;
			} else
			{
				SET_DEFERRED_TIMERS_CHECK_NEEDED;
				tpop->block_int = intrpt_ok_state;
			}
		} else
		{	/* Set system timer to wake on unexpired timer. */
			sys_timer_at = tpop->expir_time;
			sys_settimer(tpop->tid, &deltatime);
			break;	/* System timer will handle subsequent timers, so we are done. */
		}
		assert(GET_DEFERRED_TIMERS_CHECK_NEEDED);
	}
	DUMP_TIMER_INFO("At the end of start_first_timer()");
}

/* Timer handler. This is the main handler routine that is being called by the kernel upon receipt
 * of timer signal. It dispatches to the user handler routine, and removes first timer in a timer
 * queue. If the queue is not empty, it starts the first timer in the queue.
 * The "why" parameter can be DUMMY_SIG_NUM (if the handler is being invoked internally from YottaDB code)
 * or SIGALRM (if the handler is being invoked by the kernel upon receipt of the SIGALRM signal).
 */
void timer_handler(int why, siginfo_t *info, void *context, boolean_t is_os_signal_handler)
{
	int4		cmp, save_error_condition;
	GT_TIMER	*tpop, *tpop_prev = NULL;
	ABS_TIME	at;
	int		save_errno, timer_defer_cnt, orig_why;
	TID 		*deferred_tid;
	boolean_t	tid_found;
	char 		*save_util_outptr;
	va_list		save_last_va_list_ptr;
	boolean_t	util_copy_saved = FALSE, safe_for_timer_pop, signal_forwarded;
#	ifdef DEBUG
	boolean_t	save_in_nondeferrable_signal_handler;
	ABS_TIME	rel_time, old_at, late_time;
	static int	last_continue_proc_cnt = -1;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((DUMMY_SIG_NUM == why) || (SIGALRM == why));
	orig_why = why;			/* Save original value as FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED may change it */
	if (!USING_ALTERNATE_SIGHANDLING)
	{
		signal_forwarded = IS_SIGNAL_FORWARDED(sig_hndlr_generic_signal_handler, why, info);
		FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig_hndlr_timer_handler, why, NULL, info, context);
	}
	/* Now that we know FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED did not return, we hold the YDB engine lock in case this is a
	 * multi-threaded program. Therefore we can safely set the global variable "in_os_signal_handler".
	 */
	if (is_os_signal_handler)
	{
		SET_TIMER_FROM_OS;		/* Record that we were driven by an OS supplied signal */
		INCREMENT_IN_OS_SIGNAL_HANDLER;
		/* Now that we know it is the OS that delivered this "SIGALRM" signal to us, clear the global variable
		 * "timer_active" as the most common cause of this signal is the system timer that this process had
		 * already established. But note that it is possible this signal was sent externally (e.g. another process)
		 * in which case clearing the global variable is not accurate. But it is okay since the consequence of this
		 * is that we would try to start a new system timer when one already exists (the effect of that would be to
		 * overwrite the active timer with the new one thereby resulting still in one system timer in the end).
		 */
		timer_active = FALSE;				/* timer has popped; system timer not active anymore */
	}
	DUMP_TIMER_INFO("At the start of timer_handler()");
#	ifdef DEBUG
	/* Note that it is possible "in_nondeferrable_signal_handler" is non-zero if we first went into generic_signal_handler
	 * (say to handle sig-3) and then had a timer handler pop while inside there (possible for example in receiver server).
	 * So save current value of global and restore it at end of this function.
	 */
	save_in_nondeferrable_signal_handler = in_nondeferrable_signal_handler;
#	endif
	/* timer_handler() may or may not be protected from signals.
	 * If why is SIGALRM, the OS typically blocks SIGALRM while this handler is executing.
	 * If why is DUMMY_SIG_NUM, SIGALRM is not blocked, so make sure that a concurrent SIGALRM bails out at this point.
	 * All other routines which manipulate the timer data structure block SIGALRM (using SIGPROCMASK), so timer_handler()
	 * can't conflict with them. As long as those routines can't be invoked asynchronously while timer_handler (or another
	 * of those routines) is running, there can be no conflict, and the timer structures are safe from concurrent manipulation.
	 *
	 * For alternate signal handling, in which the main routine (non-M) does the signal handling and then notifies us of them
	 * occurring, this routine is similarly gated by the simple API engine lock so interference is not possible.
	 */
	if (1 < INTERLOCK_ADD(&timer_stack_count, 1))
	{
		SET_DEFERRED_TIMERS_CHECK_NEEDED;
		INTERLOCK_ADD(&timer_stack_count, -1);
		CLEAR_TIMER_FROM_OS_IF_NEEDED(is_os_signal_handler);
#		ifdef SIGNAL_PASSTHRU
		if (!USING_ALTERNATE_SIGHANDLING && ((SIGALRM == orig_why) || signal_forwarded))
		{	/* Only drive this handler if we have an actual signal - not a dummy call */
			drive_non_ydb_signal_handler_if_any("timer_handler", why, info, context, FALSE);
		}
#		endif
		DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
		return;
	}
#	ifdef DEBUG
	/* White box WBTEST_YDB_RLSIGLONGJMP slows down processing of timers to allow the conditions for the error in YDB#1065 to
	 * occur: multiple signals on top of each other in the stack cause loss of stack when siglongjmp is run.
	 */
	if (WBTEST_ENABLED(WBTEST_YDB_RLSIGLONGJMP))
	{
		if (is_os_signal_handler)
		{
			int	i;

			for (i = 0; i < 5; i++)
				SHORT_SLEEP(999);
		}
	}
#	endif
	CLEAR_DEFERRED_TIMERS_CHECK_NEEDED;
	save_errno = errno;
	save_error_condition = error_condition;		/* aka SIGNAL */
	sys_get_curr_time(&at);
	tpop = (GT_TIMER *)timeroot;
	timer_defer_cnt = 0;				/* reset the deferred timer count, since we are in timer_handler */
	safe_for_timer_pop = SAFE_FOR_TIMER_POP;
	/* If "multi_thread_in_use" is TRUE, it is possible util_out* buffers are concurrently being manipulated by the running
	 * threads. So do not use SAVE/RESTORE_UTIL_OUT_BUFFER macros. Thankfully in this case, "safe_for_timer_pop" will
	 * be FALSE (asserted below) and so only safe timer handlers will be driven. We expect the safe timer handlers to
	 * not play with the util_out* buffers. So it is actually okay to not do the SAVE/RESTORE_UTIL_OUT_BUFFER.
	 */
	assert(!multi_thread_in_use || !safe_for_timer_pop);
	if (safe_for_timer_pop)
		SAVE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	else
		save_util_outptr = NULL;
#	ifdef DEBUG
	if (safe_for_timer_pop)
		in_nondeferrable_signal_handler = IN_TIMER_HANDLER;
	/* Allow a base 100 seconds of lateness for safe timers. Note that this used to be 50 seconds before but in loaded
	 * systems we have seen the timer pop getting delayed by as much as 75 seconds. So bumped it to 100 seconds instead.
	 */
	late_time.tv_sec = 100;
	late_time.tv_nsec = 0;
#	endif
	while (tpop)					/* fire all handlers that expired */
	{
		cmp = abs_time_comp(&at, (ABS_TIME *)&tpop->expir_time);
		if (cmp < 0)
			break;
#		if defined(DEBUG) && !defined(_AIX) && !defined(__armv6l__) && !defined(__armv7l__) && !defined(__aarch64__)
		if (tpop->safe && (TREF(continue_proc_cnt) == last_continue_proc_cnt)
		    && !(ydb_white_box_test_case_enabled
			 && ((WBTEST_SIGTSTP_IN_JNL_OUTPUT_SP == ydb_white_box_test_case_number)
			     || (WBTEST_EXPECT_IO_HANG == ydb_white_box_test_case_number)
			     || (WBTEST_OINTEG_WAIT_ON_START == ydb_white_box_test_case_number))))
		{	/* Check if the timer is extremely overdue, with the following exceptions:
			 *	- Unsafe timers can be delayed indefinitely.
			 *	- AIX and ARM systems (32-bit and 64-bit) tend to arbitrarily delay processes when loaded.
			 *	- WBTEST_SIGTSTP_IN_JNL_OUTPUT_SP stops the process from running.
			 *	- Some other mechanism causes a SIGSTOP/SIGCONT, bumping continue_proc_cnt.
			 */
			rel_time = sub_abs_time(&at, (ABS_TIME *)&tpop->expir_time);
			if (abs_time_comp(&late_time, &rel_time) <= 0)
				gtm_fork_n_core();	/* Dump core, but keep going. */
		}
		last_continue_proc_cnt = TREF(continue_proc_cnt);
#		endif
		/* a) A timer might pop while we are in the non-zero intrpt_ok_state zone, which could cause collisions. Instead,
		 *    we will defer timer events and drive them once the deferral is removed, unless the timer is safe. Hence the
		 *    "safe_for_timer_pop" check below.
		 * b) "wcs_stale" is a special timer which goes through lots of heavyweight functions ("wcs_wtstart" etc.) that
		 *    can invoke functions like "syslog()/malloc()/free()" etc. all of which are a no-no while inside OS signal
		 *    handler code. A similar example is "wcs_clean_dbsync". Therefore disallow such special timer handlers if we
		 *    are inside an OS signal handler. The IS_KNOWN_UNSAFE_TIMER_HANDLER check below takes care of such functions.
		 * c) Like (b), the unsafe handler functions cannot be invoked if we are in an external call (as they could
		 *    interfere with potentially non-reentrant routines used in the external call with undesired results, GTM-8926).
		 *    Hence the "TREF(in_ext_call)" check below.
		 */
		if (tpop->safe || (safe_for_timer_pop
				   && ((!in_os_signal_handler && !TREF(in_ext_call))
				       || !IS_KNOWN_UNSAFE_TIMER_HANDLER(tpop->handler))))
		{
			if (NULL != tpop_prev)
				tpop_prev->next = tpop->next;
			else
				timeroot = tpop->next;
			if (tpop->safe)
			{
				safe_timer_cnt--;
				assert(0 <= safe_timer_cnt);
			}
			if (NULL != tpop->handler)	/* if there is a handler, call it */
			{
#				ifdef DEBUG
				if (WBTEST_ENABLED(WBTEST_DEFERRED_TIMERS)
					&& ((void *)tpop->handler != (void*)jnl_file_close_timer_ptr))
				{
					DBGFPF((stderr, "TIMER_HANDLER: handled a timer\n"));
					timer_pop_cnt++;
				}
#				endif
				timer_in_handler = TRUE;
				(*tpop->handler)(tpop->tid, tpop->hd_len, tpop->hd_data);
				timer_in_handler = FALSE;
				if (!tpop->safe)		/* if safe, avoid a system call */
				{
					DEBUG_ONLY(old_at = at);
					sys_get_curr_time(&at);	/* refresh current time if called a handler */
#					ifdef DEBUG
					/* Include the time it took to handle the unsafe timer in the allowed late time.
					 * Otherwise, a hung unsafe timer could cause a subsequent safe timer to be overdue.
					 */
					rel_time = sub_abs_time(&at, &old_at);
					late_time.tv_sec += rel_time.tv_sec;
					late_time.tv_nsec += rel_time.tv_nsec;
					if (late_time.tv_nsec > NANOSECS_IN_SEC)
					{
						late_time.tv_sec++;
						late_time.tv_nsec -= NANOSECS_IN_SEC;
					}
#					endif
				}
				DEBUG_ONLY(TRACE_TIMER_POP(tpop));
			}
			tpop->next = (GT_TIMER *)timefree;	/* put timer block on the free chain */
			timefree = tpop;
			if (NULL != tpop_prev)
				tpop = tpop_prev->next;
			else
				tpop = (GT_TIMER *)timeroot;
			num_timers_free++;
			assert(0 < num_timers_free);
		} else
		{
			timer_defer_cnt++;
#			ifdef DEBUG
			if (WBTEST_ENABLED(WBTEST_DEFERRED_TIMERS))
			{
				tid_found = FALSE;
				deferred_tid = deferred_tids;
				while (-1 != *deferred_tid)
				{
					if (*deferred_tid == tpop->tid)
					{
						tid_found = TRUE;
						break;
					}
					deferred_tid++;
					/* WBTEST_DEFERRED_TIMERS tests do not need more than TIMER_BLOCK_SIZE entries, right? */
					assert(TIMER_BLOCK_SIZE >= (deferred_tid - deferred_tids));
				}
				if (!tid_found)
				{
					*deferred_tid = tpop->tid;
					if ((void *)tpop->handler != (void*)wcs_clean_dbsync_fptr)
						DBGFPF((stderr, "TIMER_HANDLER: deferred a timer\n"));
				}
			}
#			endif
			tpop->block_int = intrpt_ok_state;
			tpop_prev = tpop;
			tpop = tpop->next;
			if ((0 == safe_timer_cnt) && !(TREF(in_ext_call) && IS_KNOWN_UNSAFE_TIMER_HANDLER(tpop_prev->handler)))
				break;		/* no more safe timers left, and not special case, so quit */
		}
	}
	if (safe_for_timer_pop)
		RESTORE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	if (safe_for_timer_pop || (0 < safe_timer_cnt))
		start_first_timer(&at, is_os_signal_handler);
	else if ((NULL != timeroot) || (0 < timer_defer_cnt))
		SET_DEFERRED_TIMERS_CHECK_NEEDED;
	/* Restore mainline error_condition global variable. This way any gtm_putmsg or rts_errors that occurred inside interrupt
	 * code do not affect the error_condition global variable that mainline code was relying on. For example, not doing this
	 * restore caused the update process (in updproc_ch) to issue a GTMASSERT (GTM-7526). BYPASSOK.
	 */
	SET_ERROR_CONDITION(save_error_condition);	/* restore error_condition & severity */
	errno = save_errno;			/* restore mainline errno by similar reasoning as mainline error_condition */
	INTERLOCK_ADD(&timer_stack_count, -1);
	CLEAR_TIMER_FROM_OS_IF_NEEDED(is_os_signal_handler);
#	ifdef DEBUG
	if (safe_for_timer_pop)
		in_nondeferrable_signal_handler = save_in_nondeferrable_signal_handler;
#	endif
#	ifdef SIGNAL_PASSTHRU
	/* If this is a call-in/simpleAPI mode and a handler exists for this signal, call it */
	if (!USING_ALTERNATE_SIGHANDLING && ((SIGALRM == orig_why) || signal_forwarded))
	{
		drive_non_ydb_signal_handler_if_any("timer_handler2", why, info, context, orig_sig_action[why].sa_sigaction);
	}
#	endif
	DUMP_TIMER_INFO("At the end of timer_handler()");
	DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
}

/* Find a timer given by tid in the timer chain.
 * Arguments:	tid	- timer id
 *		tprev	- address of pointer to previous node
 * Return:	pointer to timer in the chain, or 0 if timer is not found
 * Note:	tprev is set to the link previous to the tid link
 */
STATICFNDEF GT_TIMER *find_timer(TID tid, GT_TIMER **tprev)
{
	GT_TIMER *tc;

	tc = (GT_TIMER *)timeroot;
	*tprev = NULL;
	while (tc)
	{
		if (tc->tid == tid)
			return tc;
		*tprev = tc;
		tc = tc->next;
	}
	return 0;
}

/* Add timer to timer chain. Allocate a new link for a timer. Convert time to expiration into absolute time.
 * Insert new link into chain in timer order.
 * Arguments:	tid		- timer id
 *		time_to_expir	- elapsed time to expiration
 *		handler		- pointer to handler routine
 *      	hdata_len       - length of data to follow
 *      	hdata   	- data to pass to timer rtn if any
 *      	safe_timer	- timer's handler is in safe_handlers array
 */
STATICFNDEF GT_TIMER *add_timer(ABS_TIME *atp, TID tid, uint8 time_to_expir, void (*handler)(), int4 hdata_len,
	void *hdata, boolean_t safe_timer)
{
	GT_TIMER	*tp, *tpp, *ntp, *lastntp;
	int4		cmp, i;
	st_timer_alloc	*new_alloc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* assert that no timer entry with the same "tid" exists in the timer chain */
	assert(NULL == find_timer(tid, &tpp));
	assert(GT_TIMER_INIT_DATA_LEN >= hdata_len);
	/* obtain a new timer block */
	ntp = (GT_TIMER *)timefree;
	lastntp = NULL;
	for ( ; NULL != ntp; )
	{	/* we expect all callers of timer functions to not require more than 8 bytes of data; any violations
		 * of this assumption need to be caught---hence the assert below
		 */
		assert(GT_TIMER_INIT_DATA_LEN == ntp->hd_len_max);
		assert(ntp->hd_len_max >= hdata_len);
		if (ntp->hd_len_max >= hdata_len)	/* found one that can hold our data */
		{	/* dequeue block */
			if (NULL == lastntp)		/* first one on queue */
				timefree = ntp->next;	/* dequeue 1st element */
			else				/* is not 1st on queue -- use simple dequeue */
				lastntp->next = ntp->next;
			assert(0 < num_timers_free);
			num_timers_free--;
			break;
		}
		lastntp = ntp;	/* still looking, try next block */
		ntp = ntp->next;
	}
	/* if didn't find one, fail if dbg; else malloc a new one */
	if (NULL == ntp)
	{
		assert(FALSE);							/* if dbg, we should have enough already */
		ntp = (GT_TIMER *)malloc(SIZEOF(GT_TIMER));			/* if we are in a timer, malloc may error out */
		new_alloc = (st_timer_alloc *)malloc(SIZEOF(st_timer_alloc));	/* insert in front of the list */
		new_alloc->addr = ntp;
		new_alloc->next = (st_timer_alloc *)timer_allocs;
		timer_allocs = new_alloc;
		assert(GT_TIMER_INIT_DATA_LEN == hdata_len);
		ntp->hd_len_max = hdata_len;
	}
	ntp->tid = tid;
	ntp->handler = handler;
	if (safe_timer)
	{
		ntp->safe = TRUE;
		safe_timer_cnt++;
		assert(0 < safe_timer_cnt);
	} else
		ntp->safe = FALSE;
	ntp->block_int = INTRPT_OK_TO_INTERRUPT;
	ntp->hd_len = hdata_len;
	if (0 < hdata_len)
	{
		assert(GT_TIMER_INIT_DATA_LEN >= hdata_len);
		memcpy(ntp->hd_data, hdata, hdata_len);
	}
	add_uint8_to_abs_time(atp, time_to_expir, &ntp->expir_time);
	ntp->start_time.tv_sec = atp->tv_sec;
	ntp->start_time.tv_nsec = atp->tv_nsec;
	tp = (GT_TIMER *)timeroot;
	tpp = NULL;
	while (tp)
	{
		cmp = abs_time_comp(&tp->expir_time, &ntp->expir_time);
		if (cmp >= 0)
			break;
		tpp = tp;
		tp = tp->next;
	}
	ntp->next = tp;
	if (NULL == tpp)
		timeroot = ntp;
	else
		tpp->next = ntp;
	return ntp;
}

/* Remove timer from the timer chain. */
STATICFNDEF void remove_timer(TID tid)
{
	GT_TIMER *tprev, *tp, *tpp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DUMP_TIMER_INFO("At the start of remove_timer()");
	if ((tp = find_timer(tid, &tprev)))		/* Warning: assignment */
	{
		if (tprev)
			tprev->next = tp->next;
		else
		{
			timeroot = tp->next;
			if (NULL == timeroot)
				CLEAR_DEFERRED_TIMERS_CHECK_NEEDED;	/* assert in fast path of "clear_timers" relies on this */
		}
		if (tp->safe)
			safe_timer_cnt--;
		tp->next = (GT_TIMER *)timefree;	/* place element on free queue */
		timefree = tp;
		num_timers_free++;
		assert(0 < num_timers_free);
		/* assert that no duplicate timer entry with the same "tid" exists in the timer chain */
		assert((NULL == find_timer(tid, &tpp)));
	}
	DUMP_TIMER_INFO("After invoking remove_timer()");
}

/* System call to cancel timer. Not static because can be called from generic_signal_handler() to stop timers
 * from popping yet preserve the blocks so gtmpcat can pick them out of the core. Note that once we exit,
 * timers are cleared at the top of the exit handler.
 */
void sys_canc_timer()
{
	int	save_errno;
#	ifndef YDB_USE_POSIX_TIMERS
	struct itimerval zero;

	memset(&zero, 0, SIZEOF(struct itimerval));
#	else
	assert(posix_timer_created);
#	endif
	assert(timer_active);
	/* In case of canceling the system timer, we do not care if we succeed. Consider the two scenarios:
	 *   1) The process is exiting, so all timers must have been removed anyway, and regardless of whether the system
	 *      timer got unset or not, no handlers would be processed (even in the event of a pop).
	 *   2) Some timer is being canceled as part of the runtime logic. If the system is experiencing problems, then the
	 *      following attempt to schedule a new timer (remember that we at the very least have the heartbeat timer once
	 *      database access has been established) would fail; if no other timer is scheduled, then the canceled entry
	 *      must have been removed off the queue anyway, so no processing would occur on a pop.
	 */
#	ifdef YDB_USE_POSIX_TIMERS
	if (-1 == timer_delete(posix_timer_id))
	{
		save_errno = errno;
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("timer_delete()"), CALLFROM, save_errno);
	}
	if ((MUMPS_CALLIN == invocation_mode) && !simpleThreadAPI_active && IS_SIMPLEAPI_MODE)
	{	/* This process uses YottaDB in Simple API mode. In this case, it is possible the Simple API application
		 * spawns multiple threads (but ensures serial access to the YottaDB engine). This was seen when using
		 * the YDBPython wrapper (YDB#935). In this case, we need to not just clear "posix_timer_created" but
		 * also "posix_timer_thread_id" since it could otherwise point to a dead thread-id.
		 * Hence we use the macro call below to clear both fields.
		 */
		CLEAR_POSIX_TIMER_FIELDS_IF_APPLICABLE;
	} else
	{	/* This process uses YottaDB in one of the following modes.
		 *   1) Simple Thread API	(i.e. invocation_mode = MUMPS_CALLIN)
		 *      In this case, we want to keep the non-zero "posix_timer_thread_id" as is (points to the MAIN worker
		 *      thread id).
		 *   2) yottadb -direct	(i.e. invocation_mode = MUMPS_DIRECT)
		 *   3) yottadb -run		(i.e. invocation_mode = MUMPS_RUN)
		 *   4) mupip		(i.e. invocation_mode = MUMPS_UTILTRIGR)
		 *      In cases (2), (3) and (4) above, we want to keep the non-zero "posix_timer_thread_id" as is
		 *      (points to the yottadb/mupip process id)
		 * In all of the above cases, we only need to clear the fact that a posix timer was created (now that we
		 * deleted it). Hence we do not use CLEAR_POSIX_TIMER_FIELDS_IF_APPLICABLE like in the "if" block above.
		 * But instead just clear the one variable of interest.
		 */
		assert(posix_timer_thread_id);
		posix_timer_created = FALSE;
	}
#	else
	if (-1 == setitimer(ITIMER_REAL, &zero, &old_sys_timer))
		REPORT_SETITIMER_ERROR("ITIMER_REAL", zero, FALSE, errno);
#	endif
	timer_active = FALSE;		/* no timer is active now */
}

/* Cancel all unsafe timers. */
void cancel_unsafe_timers(void)
{
	ABS_TIME	at;
	sigset_t	savemask;
	GT_TIMER	*active, *curr, *next;
	int		rc;
	DEBUG_ONLY(int4	cnt = 0;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DUMP_TIMER_INFO("At the start of cancel_unsafe_timers()");
	if (!timer_from_OS)
		SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);	/* block SIGALRM signal */
	active = curr = (GT_TIMER *)timeroot;
	while (curr)
	{	/* If the timer is unsafe, remove it from the chain. */
		next = curr->next;
		if (!curr->safe)
			remove_timer(curr->tid);
		curr = next;
		DEBUG_ONLY(cnt++;)
	}
	assert((NULL == timeroot) || (0 < safe_timer_cnt));
	if (timeroot)
	{	/* If the head of the queue has changed, or the system timer was not running, start the current first timer. */
		if ((active != timeroot) || (!timer_active))
		{
			sys_get_curr_time(&at);
			start_first_timer(&at, IS_OS_SIGNAL_HANDLER_FALSE);
		}
	} else
	{
		CLEAR_DEFERRED_TIMERS_CHECK_NEEDED;
		/* There are no timers left, but the system timer was active, so cancel it. */
		if (timer_active)
			sys_canc_timer();
	}
#	ifdef DEBUG
	if (WBTEST_ENABLED(WBTEST_DEFERRED_TIMERS))
	{
		DBGFPF((stderr, "CANCEL_ALL_TIMERS:\n"));
		DBGFPF((stderr, " Timer pops handled: %d\n", timer_pop_cnt));
		DBGFPF((stderr, " Timers canceled: %d\n", cnt));
	}
#	endif
	if (!timer_from_OS)
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
	DUMP_TIMER_INFO("After invoking cancel_unsafe_timers()");
}

/* Initialize timers. */
void init_timers()
{
	struct sigaction	act;

	if (!USING_ALTERNATE_SIGHANDLING)
	{
		memset(&act, 0, SIZEOF(act));
		sigemptyset(&act.sa_mask);
		act.sa_sigaction = ydb_os_signal_handler;	/* "ydb_os_signal_handler" will in turn invoke "timer_handler" */
		/* FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED (invoked in "timer_handler") relies
		 * on "info" and "context" being passed in.
		 */
		act.sa_flags = YDB_SIGACTION_FLAGS;
		sigaction(SIGALRM, &act, &prev_alrm_handler);
		/* Note - YDB used to verify the expected handler here (i.e. either SIG_DFL or SIG_IGN) but that is no longer
		 * valid with timer initialization being done in gtm_startup now and not waiting for the first timer. In this
		 * case, with either variant of simple API, the handler could be set for anything by a non-M main program before
		 * YDB is initialized.
		 */
	} else
	{	/* We use an alternate method to drive signals here */
		SET_ALTERNATE_SIGHANDLER(SIGALRM, &ydb_altalrm_sighandler);
	}
	first_timeset = FALSE;
}

/* Check for deferred timers. Drive any timers that have been deferred. In case the system timer is
 * disabled, launch it for the next scheduled event. This function should be called upon leaving the
 * interrupt-deferred zone.
 */
void check_for_deferred_timers(void)
{
	sigset_t	savemask;
	int		rc;
	char		*rname;

	assert(!INSIDE_THREADED_CODE(rname));	/* below code is not thread safe as it does SIGPROCMASK() etc. */
	CLEAR_DEFERRED_TIMERS_CHECK_NEEDED;
	DEBUG_ONLY(STAPI_FAKE_TIMER_HANDLER_WAS_DEFERRED);
	timer_handler(DUMMY_SIG_NUM, NULL, NULL, IS_OS_SIGNAL_HANDLER_FALSE);
}

/* Check for timer pops. If any timers are on the queue, pretend a sigalrm occurred, and we have to
 * check everything. This is mainly for use after external calls until such time as external calls
 * can use this timing facility. Current problem is that external calls are doing their own catching
 * of sigalarms that should be ours, so we end up hung.
 */
void check_for_timer_pops(boolean_t sig_handler_changed)
{
	int			rc, stolenwhen = 0;		/* 0 = no, 1 = not first, 2 = first time */
	sigset_t 		savemask;
	struct sigaction 	current_sa;
	int			save_errno = 0;

	DBGSIGSAFEFPF((stderr, "check_for_timer_pops: sig_handler_changed=%d, first_timeset=%d, timer_active=%d\n",
				sig_handler_changed, first_timeset, timer_active));
	if (!USING_ALTERNATE_SIGHANDLING)
	{	/* If managing our own signals, verify handler for SIGALRM is as it should be */
		if (sig_handler_changed)
		{
			DBGSIGSAFEFPF((stderr, "check_for_timer_pops: current_sa.sa_handler=%p\n", current_sa.sa_handler));
			sigaction(SIGALRM, NULL, &current_sa);	/* get current info */
			if (!first_timeset)
			{
				if (ydb_os_signal_handler != current_sa.sa_sigaction)	/* check if what we expected */
				{
					init_timers();
					if (!stolen_timer)
					{
						stolen_timer = TRUE;
						stolenwhen = 1;
					}
				}
			} else	/* we haven't set so should be ... */
			{
				if ((SIG_IGN != (sighandler_t)current_sa.sa_sigaction) 		/* as set by sig_init */
				    && (SIG_DFL != (sighandler_t)current_sa.sa_sigaction)) 	/* utils, compile */
				{
					if (!stolen_timer)
					{
						stolen_timer = TRUE;
						stolenwhen = 2;
					}
				}
			}
			DBGSIGSAFEFPF((stderr, "check_for_timer_pops: stolenwhen=%d\n", stolenwhen));
			SET_DEFERRED_TIMERS_CHECK_NEEDED; /* Need to invoke timer_handler because the ext call could swallow a signal */
		}
	}
	if (timeroot && (1 > timer_stack_count))
		DEFERRED_SIGNAL_HANDLING_CHECK;	/* Check for deferred timers */
	/* Now that timer handling is done, issue errors as needed */
	if (stolenwhen)
	{
		assert(!USING_ALTERNATE_SIGHANDLING);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TIMERHANDLER, 3, current_sa.sa_sigaction,
			     LEN_AND_STR(whenstolen[stolenwhen - 1]));
	}
}

/* Externally exposed routine that does a find_timer and is SIGALRM interrupt safe. */
GT_TIMER *find_timer_intr_safe(TID tid, GT_TIMER **tprev)
{
	sigset_t 	savemask;
	GT_TIMER	*tcur;
	int		rc;

	/* Before scanning timer queues, block SIGALRM signal as otherwise that signal could cause an interrupt
	 * timer routine to be driven which could in turn modify the timer queues while this mainline code is
	 * examining the very same queue. This could cause all sorts of invalid returns (of tcur and tprev)
	 * from the find_timer call below.
	 */
	if (!timer_from_OS)
		SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);
	tcur = find_timer(tid, tprev);
	if (!timer_from_OS)
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
	return tcur;
}
