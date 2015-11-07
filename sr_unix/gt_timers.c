/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
 * void sys_get_cur_time(ABS_TIME *atp)
 * 	fetch absolute time into stucture
 *
 * void hiber_start(uint4 hiber)
 *      used to sleep for hiber milliseconds
 *
 * void start_timer(TID tid, int4 time_to_expir, void (*handler)(), int4 dlen, char *data)
 *	Used to start a new timer.
 *
 * void cancel_timer(TID tid)
 *	Cancel an existing timer.
 *	Cancelling timer with tid = 0, cancels all timers.
 */

#include "mdef.h"

#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#ifdef GTM_PTHREAD
#  include <pthread.h>
#endif
#include <signal.h>
#include "gtm_time.h"
#include "gtm_string.h"
#include "gtmimagename.h"

#if (defined(__ia64) && defined(__linux__)) || defined(__MVS__)
# include "gtm_unistd.h"
#endif /* __ia64 && __linux__ or __MVS__ */
#include "gt_timer.h"
#include "wake_alarm.h"
#ifdef DEBUG
# include "wbox_test_init.h"
# include "io.h"
#endif
#if	defined(mips) && !defined(_SYSTYPE_SVR4)
# include <bsd/sys/time.h>
#else
# include <sys/time.h>
#endif
#ifndef __MVS__
# include <sys/param.h>
#endif
#include "send_msg.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "have_crit.h"
#include "util.h"
#include "sleep.h"
#if defined(__osf__)
# define HZ	CLK_TCK
#elif defined(__MVS__)
# define HZ	gtm_zos_HZ
STATICDEF int	gtm_zos_HZ = 100;	/* see prealloc_gt_timers below */
#endif

#ifdef ITIMER_REAL
# define BSD_TIMER
#else
/* check def of time() including arg - see below; should be time_t
 * (from sys/types.h) and traditionally unsigned long */
# ifndef __osf__
int4	time();
# endif
#endif

#define TIMER_BLOCK_SIZE	64	/* # of timer entries allocated initially as well as at every expansion */
#define GT_TIMER_EXPAND_TRIGGER	8	/* if the # of timer entries in the free queue goes below this, allocate more */
#define GT_TIMER_INIT_DATA_LEN	8
#define MAX_TIMER_POP_TRACE_SZ	32

#define ADD_SAFE_HNDLR(HNDLR)						\
{									\
	assert((ARRAYSIZE(safe_handlers) - 1) > safe_handlers_cnt);	\
	safe_handlers[safe_handlers_cnt++] = HNDLR;			\
}

#ifdef BSD_TIMER
STATICDEF struct itimerval sys_timer, old_sys_timer;
#endif

#define DUMMY_SIG_NUM 0			/* following can be used to see why timer_handler was called */

STATICDEF volatile GT_TIMER *timeroot = NULL;	/* chain of pending timer requests in time order */
STATICDEF boolean_t first_timeset = TRUE;
STATICDEF struct sigaction prev_alrm_handler;	/* save previous SIGALRM handler, if any */

/* Chain of unused timer request blocks */
STATICDEF volatile	GT_TIMER	*timefree = NULL;
STATICDEF volatile 	int4		num_timers_free;	/* # of timers in the unused queue */
STATICDEF		int4		timeblk_hdrlen;
STATICDEF volatile 	st_timer_alloc	*timer_allocs = NULL;

STATICDEF int 		safe_timer_cnt, timer_pop_cnt;		/* Number of safe timers in queue/popped */
STATICDEF TID 		*deferred_tids;

STATICDEF timer_hndlr	safe_handlers[MAX_TIMER_HNDLRS + 1];	/* +1 for NULL to terminate list, or can use safe_handlers_cnt */
STATICDEF int		safe_handlers_cnt;

STATICDEF boolean_t	stolen_timer = FALSE;	/* only complain once, used in check_for_timer_pops() */
STATICDEF char 		*whenstolen[] = {"check_for_timer_pops", "check_for_timer_pops first time"}; /* for check_for_timer_pops */

#ifdef DEBUG
STATICDEF int		trc_timerpop_idx;
STATICDEF GT_TIMER	trc_timerpop_array[MAX_TIMER_POP_TRACE_SZ];

# define TRACE_TIMER_POP(TIMER_INFO)							\
{											\
	memcpy(&trc_timerpop_array[trc_timerpop_idx], TIMER_INFO, SIZEOF(GT_TIMER));	\
	trc_timerpop_idx = (trc_timerpop_idx + 1) % MAX_TIMER_POP_TRACE_SZ;		\
}
#endif

/* Flag signifying timer is active. Especially useful when the timer handlers get nested. This has not been moved to a
 * threaded framework because we do not know how timers will be used with threads.
 */
GBLDEF	volatile boolean_t	timer_active = FALSE;
GBLDEF	volatile int4		timer_stack_count = 0;
GBLDEF	volatile boolean_t	timer_in_handler = FALSE;
GBLDEF	void			(*wcs_clean_dbsync_fptr)();	/* Reference to wcs_clean_dbsync() to be used in gt_timers.c. */
GBLDEF	void			(*wcs_stale_fptr)();		/* Reference to wcs_stale() to be used in gt_timers.c. */
GBLDEF 	boolean_t		deferred_timers_check_needed;	/* Indicator whether check_for_deferred_timers() should be called
								 * upon leaving deferred zone. */

GBLREF	boolean_t	blocksig_initialized;			/* Set to TRUE when blockalrm, block_ttinout, and block_sigsent are
								 * initialized. */
GBLREF	sigset_t	blockalrm;
GBLREF	sigset_t	block_ttinout;
GBLREF	sigset_t	block_sigsent;
GBLREF	boolean_t	heartbeat_started;
GBLREF	void		(*heartbeat_timer_ptr)(void);		/* Initialized only in gtm_startup(). */
GBLREF	int4		error_condition;
GBLREF	int4		outofband;
GBLREF	int		process_exiting;

error_def(ERR_TIMERHANDLER);

/* Called when a hiber_start timer pops. Set flag so a given timer will wake up (not go back to sleep). */
STATICFNDEF void hiber_wake(TID tid, int4 hd_len, int4 **waitover_flag)
{
	**waitover_flag = TRUE;
}

/* Preallocate some memory for timers. */
void gt_timers_alloc(void)
{
	int4		gt_timer_cnt;
       	GT_TIMER	*timeblk, *timeblks;
	st_timer_alloc	*new_alloc;

	/* Allocate timer blocks putting each timer on the free queue */
	assert(1 > timer_stack_count);
	timeblk_hdrlen = OFFSETOF(GT_TIMER, hd_data[0]);
	timeblk = timeblks = (GT_TIMER *)malloc((timeblk_hdrlen + GT_TIMER_INIT_DATA_LEN) * TIMER_BLOCK_SIZE);
	new_alloc = (st_timer_alloc *)malloc(SIZEOF(st_timer_alloc));
	new_alloc->addr = timeblk;
	new_alloc->next = (st_timer_alloc *)timer_allocs;
	timer_allocs = new_alloc;
	for (gt_timer_cnt = TIMER_BLOCK_SIZE; 0 < gt_timer_cnt; --gt_timer_cnt)
	{
		timeblk->hd_len_max = GT_TIMER_INIT_DATA_LEN;	/* Set amount it can store */
		timeblk->next = (GT_TIMER *)timefree;		/* Put on free queue */
		timefree = timeblk;
		timeblk = (GT_TIMER *)((char *)timeblk + timeblk_hdrlen + GT_TIMER_INIT_DATA_LEN);	/* Next! */
	}
	assert(((char *)timeblk - (char *)timeblks) == (timeblk_hdrlen + GT_TIMER_INIT_DATA_LEN) * TIMER_BLOCK_SIZE);
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

	blocksig_initialized = TRUE;	/* note the fact that blockalrm and block_sigsent are initialized */
}

/* Initialize group of timer blocks */
void prealloc_gt_timers(void)
{	/* On certain boxes SYSCONF in this function might get called earlier than
	 * the one in set_num_additional_processors(), so unset white_box_enabled
	 * for this SYSCONF to avoid issues
	 */
#	ifdef __MVS__
#	  ifdef DEBUG
	boolean_t white_box_enabled = gtm_white_box_test_case_enabled;
	if (white_box_enabled)
		gtm_white_box_test_case_enabled = FALSE;
#	  endif
	SYSCONF(_SC_CLK_TCK, gtm_zos_HZ);	/* get the real value */
#	  ifdef DEBUG
	if (white_box_enabled)
		gtm_white_box_test_case_enabled = TRUE;
#	  endif
#	endif

	/* Preallocate some timer blocks. This will be all the timer blocks we hope to need.
	 * Allocate them with 8 bytes of possible data each.
	 * If more timer blocks are needed, we will allocate them as needed.
	 */
	gt_timers_alloc();	/* Allocate timers */
	/* Now initialize the safe timers. Must be done dynamically to avoid the situation where this module always references all
	 * possible safe timers thus pulling extra stuff into executables that don't need or want it.
	 *
	 * First step, fill in the safe timers contained within this module which are always available.
	 */
	ADD_SAFE_HNDLR(&hiber_wake);		/* Resident in this module */
	ADD_SAFE_HNDLR(&hiber_start_wait_any);	/* Resident in this module */
	ADD_SAFE_HNDLR(&wake_alarm);		/* Standalone module containing on one global reference */
}

/* Get current clock time. Fill-in the structure with the absolute time of system clock.
 * Arguments:	atp - pointer to structure of absolute time
 */
void sys_get_curr_time(ABS_TIME *atp)
{
#	ifdef BSD_TIMER
	struct timeval	tv;
	struct timezone	tz;

	/* getclock or clock_gettime perhaps to avoid tz just to ignore */
	gettimeofday(&tv, &tz);
	atp->at_sec = (int4)tv.tv_sec;
	atp->at_usec = (int4)tv.tv_usec;
#	else
	atp->at_sec = time((int4 *) 0);
	atp->at_usec = 0;
#	endif
}

/* Start hibernating by starting a timer and waiting for it. */
void hiber_start(uint4 hiber)
{
	int4		waitover;
	int4		*waitover_addr;
	TID		tid;
	sigset_t	savemask;

	assertpro(1 > timer_stack_count);		/* timer services are unavailable from within a timer handler */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);	/* block SIGALRM signal */
	/* sigsuspend() sets the signal mask to 'savemask' and waits for an ALARM signal. If the SIGALRM is a member of savemask,
	 * this process will never receive SIGALRM, and it will hang indefinitely. One such scenario would be if we interrupted a
	 * timer handler with kill -15, thus getting all timer setup reset by generic_signal_handler, and the gtm_exit_handler
	 * ended up invoking hiber_start (when starting gtmsecshr server, for instance). In such situations rely on something other
	 * than GT.M timers.
	 */
	if (sigismember(&savemask, SIGALRM))
	{
		NANOSLEEP(hiber);
	} else
	{
		waitover = FALSE;			/* when OUR timer pops, it will set this flag */
		waitover_addr = &waitover;
		tid = (TID)waitover_addr;		/* unique id of this timer */
		start_timer_int((TID)tid, hiber, hiber_wake, SIZEOF(waitover_addr), &waitover_addr, TRUE);
		/* we will loop here until OUR timer pops and sets OUR flag */
		do
		{
			assert(!sigismember(&savemask, SIGALRM));
			sigsuspend(&savemask);		/* unblock SIGALRM and wait for timer interrupt */
			if (outofband)
			{
				cancel_timer(tid);
				break;
			}
		} while(FALSE == waitover);
	}
	sigprocmask(SIG_SETMASK, &savemask, NULL);	/* reset signal handlers */
}

/* Hibernate by starting a timer and waiting for it or any other timer to pop. */
void hiber_start_wait_any(uint4 hiber)
{
	sigset_t savemask;

	if (1000 > hiber)
	{
		SHORT_SLEEP(hiber);			/* note: some platforms call hiber_start */
		return;
	}
	assertpro(1 > timer_stack_count);		/* timer services are unavailable from within a timer handler */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);	/* block SIGALRM signal and set new timer */
	/* Even though theoretically it is possible for any signal other than SIGALRM to discontinue the wait in sigsuspend,
	 * the intended use of this function targets only timer-scheduled events. For that reason, assert that SIGALRMs are
	 * not blocked prior to scheduling a timer, whose delivery we will be waiting upon, as otherwise we might end up
	 * waiting indefinitely. Note, however, that the use of NANOSLEEP in hiber_start, explained in the accompanying
	 * comment, should not be required in hiber_start_wait_any, as we presently do not invoke this function in interrupt-
	 * induced code, and so we should not end up here with SIGALARMs blocked.
	 */
	assert(!sigismember(&savemask, SIGALRM));
	start_timer_int((TID)hiber_start_wait_any, hiber, NULL, 0, NULL, TRUE);
	sigsuspend(&savemask);				/* unblock SIGALRM and wait for timer interrupt */
	cancel_timer((TID)hiber_start_wait_any);	/* cancel timer block before reenabling */
	sigprocmask(SIG_SETMASK, &savemask, NULL);	/* reset signal handlers */
}

/* Wrapper function for start_timer() that is exposed for outside use. The function ensure that time_to_expir is positive. If
 * negative value or 0 is passed, set time_to_expir to SLACKTIME and invoke start_timer(). The reason we have not merged this
 * functionality with start_timer() is because there is no easy way to determine whether the function is invoked from inside
 * GT.M or by an external routine.
 * Arguments:	tid 		- timer id
 *		time_to_expir	- time to expiration in msecs
 *		handler		- pointer to handler routine
 *      	hdata_len       - length of handler data next arg
 *      	hdata           - data to pass to handler (if any)
 */
void gtm_start_timer(TID tid,
		 int4 time_to_expir,
		 void (*handler)(),
		 int4 hdata_len,
		 void *hdata)
{
	if (0 >= time_to_expir)
		time_to_expir = SLACKTIME;
	start_timer(tid, time_to_expir, handler, hdata_len, hdata);
}

/* Start the timer. If timer chain is empty or this is the first timer to expire, actually start the system timer.
 * Arguments:	tid 		- timer id
 *		time_to_expir	- time to expiration in msecs
 *		handler		- pointer to handler routine
 *      	hdata_len       - length of handler data next arg
 *      	hdata           - data to pass to handler (if any)
 */
void start_timer(TID tid,
		 int4 time_to_expir,
		 void (*handler)(),
		 int4 hdata_len,
		 void *hdata)
{
	sigset_t	savemask;
	boolean_t	safe_timer = FALSE, safe_to_add = FALSE;
	int		i;

	assertpro(0 < time_to_expir);			/* Callers should verify non-zero time */
	if (NULL == handler)
	{
		safe_to_add = TRUE;
		safe_timer = TRUE;
	} else if ((wcs_clean_dbsync_fptr == handler) || (wcs_stale_fptr == handler))
		safe_to_add = TRUE;
	else
	{
                for (i = 0; NULL != safe_handlers[i]; i++)
                        if (safe_handlers[i] == handler)
                        {
				safe_to_add = TRUE;
				safe_timer = TRUE;
                                break;
                        }
	}
	if (!safe_to_add && (process_exiting || (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state)))
	{
		assert(WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC));
		return;
	}
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);	/* block SIGALRM signal */
	start_timer_int(tid, time_to_expir, handler, hdata_len, hdata, safe_timer);
	sigprocmask(SIG_SETMASK, &savemask, NULL);	/* reset signal handlers */
}

/* Internal version of start_timer that does not protect itself, assuming this has already been done.
 * Otherwise does as explained above in start_timer.
 */
STATICFNDEF void start_timer_int(TID tid, int4 time_to_expir, void (*handler)(), int4 hdata_len, void *hdata, boolean_t safe_timer)
{
	ABS_TIME at;

 	assert(0 != time_to_expir);
	sys_get_curr_time(&at);
	if (first_timeset)
	{
		init_timers();
		first_timeset = FALSE;
	}
	/* We expect no timer with id=<tid> to exist in the timer queue currently. This is asserted in "add_timer" call below.
	 * In pro though, we'll be safe and remove any tids that exist before adding a new entry with the same tid - 2009/10.
	 * If a few years pass without the assert failing, it might be safe then to remove the PRO_ONLY code below.
	 */
#	ifndef DEBUG
	if (timeroot && (timeroot->tid == tid))
		sys_canc_timer();
	remove_timer(tid); /* Remove timer from chain */
#	endif
	/* Check if # of free timer slots is less than minimum threshold. If so, allocate more of those while it is safe to do so */
	if ((GT_TIMER_EXPAND_TRIGGER > num_timers_free) && (1 > timer_stack_count))
		gt_timers_alloc();
	add_timer(&at, tid, time_to_expir, handler, hdata_len, hdata, safe_timer);	/* Put new timer in the queue. */
	if ((timeroot->tid == tid) || !timer_active)
		start_first_timer(&at);
}

/* Uninitialize all timers, since we will not be needing them anymore. */
STATICFNDEF void uninit_all_timers(void)
{
	st_timer_alloc	*next_timeblk;

	sys_canc_timer();
	first_timeset = TRUE;
	for (; timer_allocs;  timer_allocs = next_timeblk)	/* loop over timer_allocs entries and deallocate them */
	{
		next_timeblk = timer_allocs->next;
		free(timer_allocs->addr);			/* free the timeblk */
		free((st_timer_alloc *)timer_allocs); 		/* free the container */
	}
	/* after all timers are removed, we need to set the below pointers to NULL */
	timeroot = NULL;
	timefree = NULL;
	num_timers_free = 0;
	/* empty the blockalrm and sigsent entries */
	sigemptyset(&blockalrm);
	sigemptyset(&block_sigsent);
	sigaction(SIGALRM, &prev_alrm_handler, NULL);
	timer_active = FALSE;
}

/* Cancel timer.
 * Arguments:	tid - timer id
 */
void cancel_timer(TID tid)
{
        ABS_TIME at;
	sigset_t savemask;

	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);	/* block SIGALRM signal */
	sys_get_curr_time(&at);
	if (tid == 0)
	{
		assert(process_exiting || IS_GTMSECSHR_IMAGE); /* wcs_phase2_commit_wait relies on this flag being set BEFORE
								* cancelling all timers. But secshr doesn't have it.
								*/
		cancel_all_timers();
		uninit_all_timers();
		timer_stack_count = 0;
		sigprocmask(SIG_SETMASK, &savemask, NULL);
		return;
	}
	if (timeroot && (timeroot->tid == tid))		/* if this is the first timer in the chain, stop it */
		sys_canc_timer();
	remove_timer(tid);		/* remove it from the chain */
	start_first_timer(&at);		/* start the first timer in the chain */
	sigprocmask(SIG_SETMASK, &savemask, NULL);
}

/* Clear the timers' state for the forked-off process. */
void clear_timers(void)
{
	sigset_t savemask;

	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);	/* block SIGALRM signal */
	while (timeroot)
		remove_timer(timeroot->tid);
	timer_in_handler = FALSE;
	timer_active = FALSE;
	heartbeat_started = FALSE;
	sigprocmask(SIG_SETMASK, &savemask, NULL);
	return;
}

/* System call to set timer. Time is given im msecs.
 * Arguments:	tid		- timer id
 *		time_to_expir	- time to expiration
 *		handler		- address of handler routine
 */
STATICFNDEF void sys_settimer (TID tid, ABS_TIME *time_to_expir, void (*handler)())
{
#	ifdef BSD_TIMER
	if ((time_to_expir->at_sec == 0) && (time_to_expir->at_usec < (1000000 / HZ)))
	{
		sys_timer.it_value.tv_sec = 0;
		sys_timer.it_value.tv_usec = 1000000 / HZ;
	} else
	{
		sys_timer.it_value.tv_sec = time_to_expir->at_sec;
		sys_timer.it_value.tv_usec = (gtm_tv_usec_t)time_to_expir->at_usec;
	}
	sys_timer.it_interval.tv_sec = sys_timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &sys_timer, &old_sys_timer);
#	else
	if (time_to_expir->at_sec == 0)
		alarm((unsigned)1);
	else
		alarm(time_to_expir->at_sec);
#	endif
	timer_active = TRUE;
}

/* Start the first timer in the timer chain
 * Arguments:	curr_time	- current time assumed within the function
 */
STATICFNDEF void start_first_timer(ABS_TIME *curr_time)
{
	ABS_TIME eltime, interval;
	GT_TIMER *tpop;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((1 < timer_stack_count) || (TRUE == timer_in_handler))
	{
		deferred_timers_check_needed = FALSE;
		return;
	}
	if ((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && !process_exiting)
	{
		while (timeroot)			/* check if some timer expired while this function was getting invoked */
		{
			eltime = sub_abs_time((ABS_TIME *)&timeroot->expir_time, curr_time);
			if ((0 <= eltime.at_sec) || (0 < timer_stack_count))		/* nothing has expired yet */
				break;
			timer_handler(DUMMY_SIG_NUM); 	/* otherwise, drive the handler */
		}
		if (timeroot)				/* we still have a timer to set? */
		{
			add_int_to_abs_time(&eltime, SLACKTIME, &interval);
			deferred_timers_check_needed = FALSE;
			sys_settimer(timeroot->tid, &interval, timeroot->handler);	/* set system timer */
		}
	} else if (0 < safe_timer_cnt)			/* there are some safe timers */
	{
		tpop = (GT_TIMER *)timeroot;		/* regular timers are not allowed here, so only handle safe timers */
		while (tpop)
		{
			eltime = sub_abs_time((ABS_TIME *)&tpop->expir_time, curr_time);
			if (tpop->safe)
			{
				if (0 > eltime.at_sec)	/* at least one safe timer has expired */
					timer_handler(DUMMY_SIG_NUM);			/* so, drive what we can */
				else
				{
					add_int_to_abs_time(&eltime, SLACKTIME, &interval);
					sys_settimer(tpop->tid, &interval, tpop->handler);
				}
				break;
			} else if (0 > eltime.at_sec)
				deferred_timers_check_needed = TRUE;
			tpop = tpop->next;
		}
	}
}

/* Timer handler. This is the main handler routine that is being called by the kernel upon receipt
 * of timer signal. It dispatches to the user handler routine, and removes first timer in a timer
 * queue. If the queue is not empty, it starts the first timer in the queue. The why parameter is a
 * no-op in our case, but is required to maintain compatibility with the system type of __sighandler_t,
 * which is (void*)(int).
 */
STATICFNDEF void timer_handler(int why)
{
	int4		cmp, save_error_condition;
	GT_TIMER	*tpop, *tpop_prev = NULL;
	ABS_TIME	at;
	int		save_errno, timer_defer_cnt, offset;
	TID 		*deferred_tid;
	boolean_t	tid_found;
	char 		*save_util_outptr;
	va_list		save_last_va_list_ptr;
	boolean_t	util_copy_saved = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (SIGALRM == why)
	{	/* If why is 0, we know that timer_handler() was called directly, so no need
		 * to check if the signal needs to be forwarded to appropriate thread.
		 */
		FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(SIGALRM);
	}
#	ifdef DEBUG
	if (IS_GTM_IMAGE)
	{
		tpop = find_timer((TID)heartbeat_timer_ptr, &tpop);
		assert(process_exiting || (((NULL != tpop) && heartbeat_started) || ((NULL == tpop) && !heartbeat_started)));
	}
#	endif
	if (0 < timer_stack_count)
		return;
	timer_stack_count++;
	deferred_timers_check_needed = FALSE;
	save_errno = errno;
	save_error_condition = error_condition;	/* aka SIGNAL */
	timer_active = FALSE;				/* timer has popped; system timer not active anymore */
	sys_get_curr_time(&at);
	tpop = (GT_TIMER *)timeroot;
	timer_defer_cnt = 0;				/* reset the deferred timer count, since we are in timer_handler */
	SAVE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	while (tpop)					/* fire all handlers that expired */
	{
		cmp = abs_time_comp(&at, (ABS_TIME *)&tpop->expir_time);
		if (cmp < 0)
			break;
		/* A timer might pop while we are in the non-zero intrpt_ok_state zone, which could cause collisions. Instead,
		 * we will defer timer events and drive them once the deferral is removed, unless the timer is safe.
		 */
		if (((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (FALSE == process_exiting)) || (tpop->safe))
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
				if (gtm_white_box_test_case_enabled
					&& (WBTEST_DEFERRED_TIMERS == gtm_white_box_test_case_number)
					&& ((void *)tpop->handler != (void*)heartbeat_timer_ptr))
				{
					DBGFPF((stderr, "TIMER_HANDLER: handled a timer\n"));
					timer_pop_cnt++;
				}
#				endif
				timer_in_handler = TRUE;
				(*tpop->handler)(tpop->tid, tpop->hd_len, tpop->hd_data);
				timer_in_handler = FALSE;
				if (!tpop->safe)		/* if safe, avoid a system call */
					sys_get_curr_time(&at);	/* refresh current time if called a handler */
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
			if (gtm_white_box_test_case_enabled
				&& (WBTEST_DEFERRED_TIMERS == gtm_white_box_test_case_number))
			{
				if (!deferred_tids)
				{
					deferred_tids = (TID *)malloc(SIZEOF(TID) * 2);
					*deferred_tids = tpop->tid;
					*(deferred_tids + 1) = -1;
					DBGFPF((stderr, "TIMER_HANDLER: deferred a timer\n"));
				} else
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
					}
					if (!tid_found)
					{
						offset = deferred_tid - deferred_tids;
						deferred_tid = (TID *)malloc((offset + 2) * SIZEOF(TID));
						memcpy(deferred_tid, deferred_tids, offset * SIZEOF(TID));
						free(deferred_tids);
						deferred_tids = deferred_tid;
						*(deferred_tids + offset++) = tpop->tid;
						*(deferred_tids + offset) = -1;
						DBGFPF((stderr, "TIMER_HANDLER: deferred a timer\n"));
					}
				}
			}
#			endif
			tpop_prev = tpop;
			tpop = tpop->next;
			if (0 == safe_timer_cnt)	/* no more safe timers left, so quit */
				break;
		}
	}
	RESTORE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	if (((FALSE == process_exiting) && (INTRPT_OK_TO_INTERRUPT == intrpt_ok_state)) || (0 < safe_timer_cnt))
		start_first_timer(&at);
	else if ((NULL != timeroot) || (0 < timer_defer_cnt))
		deferred_timers_check_needed = TRUE;
	/* Restore mainline error_condition global variable. This way any gtm_putmsg or rts_errors that occurred inside
	 * interrupt code do not affect the error_condition global variable that mainline code was relying on.
	 * For example, not doing this restore caused the update process (in updproc_ch) to issue a GTMASSERT (GTM-7526).
	 */
	error_condition = save_error_condition;
	errno = save_errno;		/* restore mainline errno by similar reasoning as mainline error_condition */
	timer_stack_count--;
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
STATICFNDEF void add_timer(ABS_TIME *atp, TID tid, int4 time_to_expir, void (*handler)(), int4 hdata_len,
	void *hdata, boolean_t safe_timer)
{
	GT_TIMER	*tp, *tpp, *ntp, *lastntp;
	int4		cmp, i;
	st_timer_alloc	*new_alloc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* assert that no timer entry with the same "tid" exists in the timer chain */
	assert(NULL == find_timer(tid, &tpp));
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
		ntp = (GT_TIMER *)malloc(timeblk_hdrlen + hdata_len);		/* if we are in a timer, malloc may error out */
		new_alloc = (st_timer_alloc *)malloc(SIZEOF(st_timer_alloc));	/* insert in front of the list */
		new_alloc->addr = ntp;
		new_alloc->next = (st_timer_alloc *)timer_allocs;
		timer_allocs = new_alloc;
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
	ntp->hd_len = hdata_len;
	if (0 < hdata_len)
		memcpy(ntp->hd_data, hdata, hdata_len);
	add_int_to_abs_time(atp, time_to_expir, &ntp->expir_time);
	ntp->start_time.at_sec = atp->at_sec;
	ntp->start_time.at_usec = atp->at_usec;
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
	return;
}

/* Remove timer from the timer chain. */
STATICFNDEF void remove_timer(TID tid)
{
	GT_TIMER *tprev, *tp, *tpp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (tp = find_timer(tid, &tprev))		/* Warning: assignment */
	{
		if (tprev)
			tprev->next = tp->next;
		else
			timeroot = tp->next;
		if (tp->safe)
			safe_timer_cnt--;
		tp->next = (GT_TIMER *)timefree;	/* place element on free queue */
		timefree = tp;
		num_timers_free++;
		assert(0 < num_timers_free);
		/* assert that no duplicate timer entry with the same "tid" exists in the timer chain */
		assert((NULL == find_timer(tid, &tpp)));
	}
}

/* System call to cancel timer. Not static because can be called from generic_signal_handler() to stop timers
 * from popping yet preserve the blocks so gtmpcat can pick them out of the core. Note that once we exit,
 * timers are cleared at the top of the exit handler.
 */
void sys_canc_timer()
{
#	ifdef BSD_TIMER
	struct itimerval zero;

	memset(&zero, 0, SIZEOF(struct itimerval));
	setitimer(ITIMER_REAL, &zero, &old_sys_timer);
#	else
	alarm(0);
#	endif
	timer_active = FALSE;		/* no timer is active now */
}

/* Cancel all timers.
 * Note: The timer signal must be blocked prior to entry
 */
STATICFNDEF void cancel_all_timers(void)
{
	DEBUG_ONLY(int4 cnt = 0;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (timeroot)
		sys_canc_timer();
	while (timeroot)
	{	/* remove timer from the chain */
		remove_timer(timeroot->tid);
		DEBUG_ONLY(cnt++;)
	}
	safe_timer_cnt = 0;
	if (!timeroot)
	{
		deferred_timers_check_needed = FALSE;
	}
#	ifdef DEBUG
	if (gtm_white_box_test_case_enabled
		&& WBTEST_DEFERRED_TIMERS == gtm_white_box_test_case_number)
	{
		DBGFPF((stderr, "CANCEL_ALL_TIMERS:\n"));
		DBGFPF((stderr, " Timer pops handled: %d\n", timer_pop_cnt));
		DBGFPF((stderr, " Timers canceled: %d\n", cnt));
	}
#	endif
}

/* Initialize timers. */
STATICFNDEF void init_timers()
{
	struct sigaction	act;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = timer_handler;
	sigaction(SIGALRM, &act, &prev_alrm_handler);
	if (first_timeset && 					/* not from timer_handler to prevent dup message */
	    (SIG_IGN != prev_alrm_handler.sa_handler) &&	/* as set by sig_init */
	    (SIG_DFL != prev_alrm_handler.sa_handler)) 		/* utils, compile */
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TIMERHANDLER, 3, prev_alrm_handler.sa_handler,
			LEN_AND_LIT("init_timers"));
	    	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TIMERHANDLER, 3, prev_alrm_handler.sa_handler,
			LEN_AND_LIT("init_timers"));
	    	assert(FALSE);
	}
}

/* Check for deferred timers. Drive any timers that have been deferred. In case the system timer is
 * disabled, launch it for the next scheduled event. This function should be called upon leaving the
 * interrupt-deferred zone.
 */
void check_for_deferred_timers(void)
{
	sigset_t savemask;

	deferred_timers_check_needed = FALSE;
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);	/* block SIGALRM signal */
	timer_handler(DUMMY_SIG_NUM);
	sigprocmask(SIG_SETMASK, &savemask, NULL);	/* reset signal handlers */
}

/* Check for timer pops. If any timers are on the queue, pretend a sigalrm occurred, and we have to
 * check everything. This is mainly for use after external calls until such time as external calls
 * can use this timing facility. Current problem is that external calls are doing their own catching
 * of sigalarms that should be ours, so we end up hung.
 */
void check_for_timer_pops()
{
	int			stolenwhen = 0;		/* 0 = no, 1 = not first, 2 = first time */
	sigset_t 		savemask;
	struct sigaction 	current_sa;

	sigaction(SIGALRM, NULL, &current_sa);	/* get current info */
	if (!first_timeset)
	{
		if (timer_handler != current_sa.sa_handler)	/* check if what we expected */
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
		if ((SIG_IGN != current_sa.sa_handler) &&	/* as set by sig_init */
		    (SIG_DFL != current_sa.sa_handler)) 	/* utils, compile */
		{
			if (!stolen_timer)
			{
				stolen_timer = TRUE;
				stolenwhen = 2;
			}
		}
	}
	if (timeroot && (1 > timer_stack_count))
	{
		sigprocmask(SIG_BLOCK, &blockalrm, &savemask);	/* block SIGALRM signal */
		timer_handler(DUMMY_SIG_NUM);
		sigprocmask(SIG_SETMASK, &savemask, NULL);	/* reset signal handlers */
	}
	if (stolenwhen)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TIMERHANDLER, 3, current_sa.sa_handler,
			LEN_AND_STR(whenstolen[stolenwhen - 1]));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TIMERHANDLER, 3, current_sa.sa_handler,
			LEN_AND_STR(whenstolen[stolenwhen - 1]));
		assert(FALSE);					/* does not return here */
	}
}

/* Externally exposed routine that does a find_timer and is SIGALRM interrupt safe. */
GT_TIMER *find_timer_intr_safe(TID tid, GT_TIMER **tprev)
{
	sigset_t 	savemask;
	GT_TIMER	*tcur;

	/* Before scanning timer queues, block SIGALRM signal as otherwise that signal could cause an interrupt
	 * timer routine to be driven which could in turn modify the timer queues while this mainline code is
	 * examining the very same queue. This could cause all sorts of invalid returns (of tcur and tprev)
	 * from the find_timer call below.
	 */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);
	tcur = find_timer(tid, tprev);
	sigprocmask(SIG_SETMASK, &savemask, NULL);
	return tcur;
}
