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

/*
 * --------------------------------------------------------------
 * This file contains a general purpose timer package.
 * Simultaneous multiple timers are supported.
 * All outstanding timers are contained in a queue of
 * pending requests. New timer is added to the queue in an
 * expiration time order. The first timer in a queue expires
 * first, and the last one expires last.
 * When timer expires, the signal is generated and the process
 * is awakened. This timer is then removed from the queue,
 * and the first timer in a queue is started again, and so on.
 * Starting a timer with the timer id equal to one of the existing
 * timers in a chain will remove the existing timer from the chain
 * and add a new one instead.
 *
 * It is a responsibility of the user to go to hibernation mode
 * by executing appropriate system call if the user needs to
 * wait for the timer expiration.
 *
 * Following routines are top level, user callable
 * routines of this package:
 *
 * uninit_timers()
 *	De-Initialize timers - restore signals, etc.
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
 *	Cancelling timer with tid = 0, cancells all timers.
 * --------------------------------------------------------------
 */

#include "mdef.h"

#include <errno.h>
#include "gtm_time.h"
#include "gtm_string.h"
#include <stddef.h>

#if (defined(__ia64) && defined(__linux__)) || defined(__MVS__)
#include "gtm_unistd.h"
#endif /* __ia64 && __linux__ or __MVS__ */

#include "gt_timer.h"
#include "wake_alarm.h"

#if	defined(mips) && !defined(_SYSTYPE_SVR4)
#include <bsd/sys/time.h>
#else
#include <sys/time.h>
#endif

#ifndef __MVS__
#include <sys/param.h>
#endif
#include "send_msg.h"


#if defined(__osf__)
#define HZ	CLK_TCK
#elif defined(__MVS__)
#define HZ	gtm_zos_HZ
STATICDEF int	gtm_zos_HZ = 100;	/* see prealloc_gt_timers below */
#endif

#ifdef ITIMER_REAL
#define BSD_TIMER
#else

/* check def of time() including arg - see below   should be time_t
       (from sys/types.h) and traditionally unsigned long */
#ifndef __osf__
int4	time();
#endif
#endif

/* Set each timer request to go for 10ms more than requested, since the
 * interval timer alarm will sometimes go off early on many UNIX systems
 * 10ms is more than enough for all systems tested so far (SunOS, Solaris,
 * HP/UX, NonStop/UX)
 */
#ifndef SLACKTIME
# define SLACKTIME	10
#endif
#define TIMER_BLOCK_SIZE	64	/* # of timer entries allocated initially as well as at every expansion */
#define GT_TIMER_EXPAND_TRIGGER	8	/* if the # of timer entries in the free queue goes below this, allocate more */
#define GT_TIMER_INIT_DATA_LEN	8

#ifdef BSD_TIMER
static struct itimerval sys_timer, old_sys_timer;
#endif

/* following can be used to see why timer_handler was called */
#define DUMMY_SIG_NUM 0

volatile STATICDEF GT_TIMER *timeroot = NULL;	/* Chain of pending timer requests in time order */
static boolean_t first_timeset = TRUE;
/*
 * Chain of unused timer request blocks
 */
volatile static GT_TIMER	*timefree = NULL;
volatile static int4		num_timers_free;	/* # of timers in the unused queue */
static		int4		timeblk_hdrlen;
GBLREF	boolean_t	blocksig_initialized;	/* set to TRUE when blockalrm and block_sigsent are initialized */
GBLREF	sigset_t	blockalrm;
GBLREF	sigset_t	block_sigsent;
volatile static st_timer_alloc	*timer_allocs = NULL;
/*
 * Save previous SIGALRM handler if any.
 */
static struct sigaction prev_alrm_handler;
/*
 * Flag signifying timer is active. Especially useful
 * when the timer handlers get nested..
 */
volatile static int4 timer_active = FALSE;

GBLDEF volatile boolean_t timer_in_handler = FALSE;     /* set to TRUE when timer pops */

GBLREF	int4		outofband;
GBLREF	int		process_exiting;

static void (*safe_handlers[])() = {hiber_wake, wake_alarm , NULL};

error_def(ERR_TIMERHANDLER);

/*
 * --------------------------------------
 * Uninitialize timers and signals
 * --------------------------------------
 */
void	uninit_timers(void)
{
	/* restore previous handler */
	sigaction(SIGALRM, &prev_alrm_handler, NULL);
}

/*
 * --------------------------------------
 * Called when a hiber_start timer pops.
 * Set flag so a given timer will wake up
 *  (not go back to sleep).
 * --------------------------------------
 */
STATICFNDEF void hiber_wake(TID tid, int4 hd_len, int4 **waitover_flag)
{
	**waitover_flag = TRUE;
}

STATICFNDEF void gt_timers_alloc(void)
{
	int4		gt_timer_cnt;
       	GT_TIMER	*timeblk, *timeblks;
	st_timer_alloc	*new_alloc;

	assert(!timer_in_handler);
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

/*
 * Do the initialization of block_sigsent and blockalrm, and set blocksig_initialized to TRUE, so
 * that we can later block signals when there is a need. This function should be called very early
 * in the main() routines of modules that wish to do their own interrupt handling.
 */
void set_blocksig(void)
{
	sigemptyset(&blockalrm);
	sigaddset(&blockalrm, SIGALRM);

	sigemptyset(&block_sigsent);
	sigaddset(&block_sigsent, SIGINT);
	sigaddset(&block_sigsent, SIGQUIT);
	sigaddset(&block_sigsent, SIGTERM);
	sigaddset(&block_sigsent, SIGTSTP);
	sigaddset(&block_sigsent, SIGCONT);
	sigaddset(&block_sigsent, SIGALRM);

	blocksig_initialized = TRUE;	/* note the fact that blockalrm and block_sigsent are initialized */
}

/*
 * --------------------------------------
 * Initialize group of timer blocks
 * --------------------------------------
 */
void prealloc_gt_timers(void)
{

	/* Preallocate some timer blocks. This will be all the timer blocks we hope to need.
	 * Allocate them with 8 bytes of possible data each.
	 * If more timer blocks are needed, we will allocate them as needed.
	 */
#ifdef __MVS__
	gtm_zos_HZ == sysconf(_SC_CLK_TCK);	/* get the real value */
#endif

	gt_timers_alloc();	/* Allocate timers */
}

/*
 * ----------------------------------------------------
 * Get current clock time
 *	Fill-in the structure with the absolute time
 *	of system clock.
 *
 * Arguments:
 *	atp	- pointer to structure of absolute time
 * ----------------------------------------------------
 */
void	sys_get_curr_time (ABS_TIME *atp)
{
#ifdef BSD_TIMER
	struct timeval	tv;
	struct timezone	tz;
	struct tm	*dtp;

	/* getclock or clock_gettime perhaps to avoid tz just to ignore */
	gettimeofday(&tv, &tz);
	atp->at_sec = (int4)tv.tv_sec;
	atp->at_usec = (int4)tv.tv_usec;
#else
	atp->at_sec = time((int4 *) 0);
	atp->at_usec = 0;
#endif
}

/*
 * ---------------------------------------------------------
 * Start hibernating by starting a timer and waiting for it.
 * ---------------------------------------------------------
 */

void	hiber_start (uint4 hiber)
{
	/* start_timer_int has char * as hdata type */
	int4		waitover;
	int4		*waitover_addr;
	TID		tid;
	sigset_t	savemask;

	/* Timer services are unavailable from within a timer handler */
	if (timer_in_handler)
		GTMASSERT;

	/* block SIGALRM signal */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);

	waitover = FALSE;		/* When OUR timer pops, it will set this flag */
	waitover_addr = &waitover;
	tid = (TID)waitover_addr;	/* Unique id of this timer */
	start_timer_int((TID)tid, hiber, hiber_wake, SIZEOF(waitover_addr), &waitover_addr);

	/* We will loop here until OUR timer pops and sets OUR flag. Otherwise
	   we will keep waiting for it. */
	do
	{	/* unblock SIGALRM and wait for timer interrupt */
		sigsuspend(&savemask);

		if (outofband)
		{
                        cancel_timer(tid);
			break;
		}
	} while(FALSE == waitover);

	/* reset signal handlers */
	sigprocmask(SIG_SETMASK, &savemask, NULL);
}

/*
 * ---------------------------------------------------------
 * Start hibernating by starting a timer and waiting for it
 * or any other timer interrupt that happens to come along.
 * ---------------------------------------------------------
 */

void	hiber_start_wait_any(uint4 hiber)
{
	sigset_t	savemask;

	if (1000 > hiber)
	{
		SHORT_SLEEP(hiber);	/* note: some platforms call hiber_start */
		return;
	}

	/* Timer services are unavailable from within a timer handler */
	if (timer_in_handler)
		GTMASSERT;

	/* block SIGALRM signal and set new timer */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);
	start_timer_int((TID)hiber_start_wait_any, hiber, NULL, 0, NULL);

	/* unblock SIGALRM and wait for timer interrupt */
	sigsuspend(&savemask);

	/* Cancel timer block before reenabling */
	cancel_timer((TID)hiber_start_wait_any);

	/* reset signal handlers */
	sigprocmask(SIG_SETMASK, &savemask, NULL);
}

/*
 * ------------------------------------------------------
 * Start the timer
 *	If timer chain is empty or this is the first timer
 *	to expire, actually start the timer.
 *
 * Arguments:
 *	tid		- timer id
 *	time_to_expir	- time to expiration in msecs;
 *	handler		- pointer to handler routine
 *      hdata_len       - length of handler data next arg
 *      hdata           - data to pass to handler (if any)
 * ------------------------------------------------------
 */
void start_timer(TID tid,
		 int4 time_to_expir,
		 void (*handler)(),
		 int4 hdata_len,
		 void *hdata)
{
	sigset_t savemask;

	if (0 >= time_to_expir)
		GTMASSERT;

	/* block SIGALRM signal */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);

	start_timer_int(tid, time_to_expir, handler, hdata_len, hdata);

	/* reset signal handlers */
	sigprocmask(SIG_SETMASK, &savemask, NULL);
}

/*
 * ------------------------------------------------------
 * Internal version of start_timer that does not protect
 * itself assuming this has already been done. Otherwise
 * does as explained above in start_timer.
 * ------------------------------------------------------
 */
STATICFNDEF void start_timer_int(TID tid, int4 time_to_expir, void (*handler)(), int4 hdata_len, void *hdata)
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
	PRO_ONLY(
		if (timeroot && (timeroot->tid == tid))
			sys_canc_timer(tid);
		remove_timer(tid); /* Remove timer from chain */
	)
	/* Check if # of free timer slots is less than minimum threshold. If so allocate more of those while it is safe to do so */
	if ((GT_TIMER_EXPAND_TRIGGER > num_timers_free) && !timer_in_handler)
		gt_timers_alloc();
	/* Link new timer into timer chain */
	add_timer(&at, tid, time_to_expir, handler, hdata_len, hdata);
	if ((timeroot->tid == tid) || !timer_active)
		start_first_timer(&at);
}

STATICFNDEF void uninit_all_timers(void)
{
	st_timer_alloc	*next_timeblk;
	st_timer_alloc *timer_iter;

#ifdef BSD_TIMER
	/* clear timer */
	sys_timer.it_interval.tv_sec = sys_timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &sys_timer, &old_sys_timer);
	old_sys_timer.it_interval.tv_sec = old_sys_timer.it_interval.tv_usec = 0;
#else
	alarm((unsigned)1);
#endif
	first_timeset = TRUE;
        /* Loop over timer_allocs entries and deallocate them */
	for (; timer_allocs;  timer_allocs = next_timeblk)
	{
		next_timeblk = timer_allocs->next;
		free(timer_allocs->addr);		/* Free the timeblk */
		free((st_timer_alloc *)timer_allocs); 	/* Free the container */
	}
	/* After all timers are removed, we need to set the below pointers to NULL */
	timeroot = NULL;
	timefree = NULL;
	num_timers_free = 0;
	/* Empty the blockalrm and sigsent entries */
	sigemptyset(&blockalrm);
	sigemptyset(&block_sigsent);
	uninit_timers();
	timer_active = FALSE;
}
/*
 * ---------------------------------------------
 * Cancel timer
 *
 * Arguments:
 *	tid	- timer id
 * ---------------------------------------------
 */
void cancel_timer(TID tid)
{
        ABS_TIME at;
	sigset_t savemask;

	/* block SIGALRM signal */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);

	sys_get_curr_time(&at);
	if (tid == 0)
	{
		assert(process_exiting); /* wcs_phase2_commit_wait relies on this flag being set BEFORE cancelling all timers */
		cancel_all_timers();
		uninit_all_timers();
		timer_in_handler = FALSE;
		sigprocmask(SIG_SETMASK, &savemask, NULL);
		return;
	}

	/* If this is the first timer in the chain, stop it */
	if (timeroot && timeroot->tid == tid)
		sys_canc_timer(tid);

	/* remove it from the chain */
	remove_timer(tid);

	/* Start the first timer in the chain */
	start_first_timer(&at);

	sigprocmask(SIG_SETMASK, &savemask, NULL);
}

/*
 * ---------------------------------------------
 * Clear the timers' state for the forked-off process.
 * ---------------------------------------------
 */
void clear_timers(void)
{
	sigset_t savemask;

	/* block SIGALRM signal */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);
	while (timeroot)
		remove_timer(timeroot->tid);
	timer_in_handler = FALSE;
	timer_active = FALSE;
	sigprocmask(SIG_SETMASK, &savemask, NULL);
	return;
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
STATICFNDEF void	sys_settimer (TID tid, ABS_TIME *time_to_expir, void (*handler)())
{
#ifdef BSD_TIMER
	if (time_to_expir->at_sec == 0 && time_to_expir->at_usec < (1000000 / HZ))
	{
		sys_timer.it_value.tv_sec = 0;
		sys_timer.it_value.tv_usec = 1000000 / HZ;
	}
	else
	{
		sys_timer.it_value.tv_sec = time_to_expir->at_sec;
		sys_timer.it_value.tv_usec = (gtm_tv_usec_t)time_to_expir->at_usec;
	}
	sys_timer.it_interval.tv_sec = sys_timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &sys_timer, &old_sys_timer);
#else
	if (time_to_expir->at_sec == 0)
		alarm((unsigned)1);
	else
		alarm(time_to_expir->at_sec);
#endif
	timer_active = TRUE;
}


/*
 * ----------------------------------------
 * Start the first timer in the timer chain
 * ----------------------------------------
 */
STATICFNDEF void start_first_timer(ABS_TIME *curr_time)
{
	ABS_TIME eltime, interval;

	if (FALSE == timer_in_handler)
	{
		/* smw why loop timeroot here and timer_handler */
		/* se: while I don't recall the specific purpose of this loop, it had to do with other
		   than timer_handler cases where something came due while we were out doing other
		   things. It may have been a "performance" enhancement trying to drive stuff that had
		   come due without waiting for the timer pop to drive it -- Anyway, we don't want this
		   particular code to be run from code inside a handler since the handler return will do
		   this. Without the timer_in_handler check, busy interrupt handlers can nest so deeply
		   that we run out of timer blocks and have a very long stack.
		*/
		while (timeroot)
		{
			eltime = sub_abs_time((ABS_TIME *)&timeroot->expir_time, curr_time);
			if (0 > eltime.at_sec)		/* First entry on queue expired? */
				timer_handler(DUMMY_SIG_NUM);	/* Invoke handlers -- we are blocked on sigalarms here */
			else
				break;			/* eltime ok -- */
		}
		if (timeroot)	/* We still have a timer to set ? */
		{
			add_int_to_abs_time(&eltime, SLACKTIME, &interval);
			/* Call system to set timer */
			sys_settimer(timeroot->tid, &interval, timeroot->handler);
		}
	}
}


/*
 * ------------------------------------------------------
 * Timer handler.
 *	This is the main handler routine that is being
 *	called by the kernel upon receipt of timer signal.
 *	It dispatches to the user handler routine, and
 *	removes first timer in a timer queue.
 *	If the queue is not empty, it starts the first timer
 *	in the queue.
 *
 * ------------------------------------------------------
 */
STATICFNDEF void timer_handler(int why)
{
	int4		cmp;
	GT_TIMER	*tpop;
	ABS_TIME	at;
	sigset_t	savemask;
	int4		eltime;
	int		save_errno;
	boolean_t	save_timer_in_handler;

	save_errno = errno;
	timer_active = FALSE;				/* Timer has popped - not active anymore */
	save_timer_in_handler = timer_in_handler;	/* since recurses */
	timer_in_handler = TRUE;	/* but we are (usually) at interrupt level */
	sys_get_curr_time(&at);

	/* Fire all handlers that expired */
	while (timeroot)
	{
		cmp = abs_time_comp(&at, (ABS_TIME *)&timeroot->expir_time);
		if (cmp < 0)
			break;
		else
		{	/* Delete first entry in a timer chain */

			tpop = (GT_TIMER *)timeroot;
			timeroot = tpop->next;

			if (NULL != tpop->handler)	/* If want a handler, do it */
			{
				(*tpop->handler)(tpop->tid, tpop->hd_len, tpop->hd_data);

				if (!tpop->safe)	/* If safe, avoid get environment call */
					sys_get_curr_time(&at);   /* Refresh current time if called a handler */
			}

			/* put timer block on the free chain */
			tpop->next = (GT_TIMER *)timefree;
			timefree = tpop;
			num_timers_free++;
			assert(0 < num_timers_free);
		}
	}

	/* Start the first timer in the chain */
	timer_in_handler = save_timer_in_handler;
	start_first_timer(&at);
	errno = save_errno;		/* restore mainline errno */
}


/*
 * ------------------------------------------------------
 * Find a timer given by tid in the timer chain
 *
 * Arguments:
 *	tid	- timer id
 *	tprev	- address of pointer to previous node
 *
 * Return:
 *	pointer to timer in the chain, or
 *	0 - if timer does not exist on the chain
 *
 * Side effects:
 *	tprev is set to the link previous to the tid link
 * ------------------------------------------------------
 */
STATICFNDEF GT_TIMER *find_timer(TID tid, GT_TIMER **tprev)
{
	GT_TIMER *tp, *tc;

	tc = (GT_TIMER*)timeroot;
	*tprev = 0;
	while (tc)
	{
		if (tc->tid == tid)
			return(tc);
		*tprev = tc;
		tc = tc->next;
	}
	return (0);
}

/*
 * --------------------------------------------------
 * Add timer to timer chain
 *	Allocate a new link for a timer.
 *	Convert time to expiration into absolute time.
 *	Insert new link into chain in timer order.
 *
 * Arguments:
 *	tid		- timer id
 *	time_to_expir	- elapsed time to expiration
 *	handler		- pointer to handler routine
 *      hdata_len       - length of data to follow
 *      hdata   	- data to pass to timer rtn if any
 *
 * Return:
 *	TRUE	- timer added ok
 *	FALSE	- timer could not be added
 * --------------------------------------------------
 */
STATICFNDEF void add_timer(ABS_TIME *atp, TID tid, int4 time_to_expir, void (*handler)(), int4 hdata_len, void *hdata)
{
	GT_TIMER	*tp, *tpp, *ntp, *lastntp;
	int4		cmp, i;
	st_timer_alloc	*new_alloc;

	/* Assert that no timer entry with the same "tid" exists in the timer chain */
	assert((NULL == find_timer(tid, &tpp)));
	/* Obtain a new timer block */
	ntp = (GT_TIMER *)timefree;			/* Start at first free block */
	lastntp = NULL;
	for ( ; NULL != ntp; )
	{	/* We expect all callers of timer functions to not require more than 8 bytes of data. Any violations
		 * of this assumption need to be caught hence the assert below.
		 */
		assert(GT_TIMER_INIT_DATA_LEN == ntp->hd_len_max);
		assert(ntp->hd_len_max >= hdata_len);
		if (ntp->hd_len_max >= hdata_len)	/* Found one that can hold our data */
		{	/* Dequeue block */
			if (NULL == lastntp)		/* First one on queue */
				timefree = ntp->next;	/* Dequeue 1st element */
			else /* is not 1st on queue -- use simple dequeue */
				lastntp->next = ntp->next;
			assert(0 < num_timers_free);
			num_timers_free--;
			break;
		}
		/* Still looking, try next block */
		lastntp = ntp;
		ntp = ntp->next;
	}
	/* If didn't find one, fail if dbg; else malloc a new one */
	if (NULL == ntp)
	{
		assert(FALSE);		/* If dbg, we should have enough already */
		ntp = (GT_TIMER *)malloc(timeblk_hdrlen + hdata_len); /* if we are in a timer, this malloc may error out */
		/* Insert in front of the list */
		new_alloc = (st_timer_alloc *)malloc(SIZEOF(st_timer_alloc));
		new_alloc->addr = ntp;
		new_alloc->next = (st_timer_alloc *)timer_allocs;
		timer_allocs = new_alloc;
		ntp->hd_len_max = hdata_len;
	}
	ntp->tid = tid;
	ntp->handler = handler;
	ntp->safe = FALSE;
        if (NULL == handler)
                ntp->safe = TRUE;
        else
	{
                for (i = 0; NULL != safe_handlers[i]; i++)
		{
                        if (safe_handlers[i] == handler)
                        {
                                ntp->safe = TRUE;   /* known to just set flags, etc. */
                                break;
                        }
		}
	}
	ntp->hd_len = hdata_len;
	if (0 < hdata_len)
		memcpy(ntp->hd_data, hdata, hdata_len);
	add_int_to_abs_time(atp, time_to_expir, &ntp->expir_time);

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

/*
 * ---------------------------------------------
 * Remove timer from the timer chain
 * ---------------------------------------------
 */
STATICFNDEF void remove_timer(TID tid)
{
	GT_TIMER *tprev, *tp, *tpp;

	if ((tp = find_timer(tid, &tprev)))
	{
		if (tprev)
			tprev->next = tp->next;
		else
			timeroot = tp->next;
		/* Place element on free queue */
		tp->next = (GT_TIMER *)timefree;
		timefree = tp;
		num_timers_free++;
		assert(0 < num_timers_free);
		/* Assert that no duplicate timer entry with the same "tid" exists in the timer chain */
		assert((NULL == find_timer(tid, &tpp)));
	}
}

/*
 * ---------------------------------------------
 * System call to cancel timer.
 * ---------------------------------------------
 */
STATICFNDEF void	sys_canc_timer(TID tid)
{
#ifdef BSD_TIMER
	struct itimerval zero;
	memset(&zero, 0, SIZEOF(struct itimerval));
	setitimer(ITIMER_REAL, &zero, &old_sys_timer);
#else
	alarm(0);
#endif
	timer_active = FALSE;		/* No timer is active now */
}

/*
 * ---------------------------------------------
 * Cancel all timers
 *
 * Arguments:
 *	none
 *
 * Dependencies:
 *     The timer signal must be blocked prior to entry
 * ---------------------------------------------
 *
 */
STATICFNDEF void cancel_all_timers(void)
{
	if (timeroot)
		sys_canc_timer(timeroot->tid);

	while (timeroot)
		/* Remove timer from the chain */
		remove_timer(timeroot->tid);
}

/*
 * --------------------------------------
 * Initialize timers
 * --------------------------------------
 */
STATICFNDEF void	init_timers()
{
	struct sigaction	act;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = timer_handler;
	sigaction(SIGALRM, &act, &prev_alrm_handler);
	if (first_timeset && 				/* not from timer_handler to prevent dup message */
	    (SIG_IGN != prev_alrm_handler.sa_handler) && /* as set by sig_init */
	    (SIG_DFL != prev_alrm_handler.sa_handler))  /* utils, compile */
	{
		send_msg(VARLSTCNT(5) ERR_TIMERHANDLER, 3, prev_alrm_handler.sa_handler,
			LEN_AND_LIT("init_timers"));
	    	rts_error(VARLSTCNT(5) ERR_TIMERHANDLER, 3, prev_alrm_handler.sa_handler,
			LEN_AND_LIT("init_timers"));
	    	assert(FALSE);
	}
}


/*
 * ---------------------------------------
 * Check for timer pops. If any timers are
 * on the queue, pretend a sigalrm occur'd
 * and we have to check everything. This
 * is mainly for use after external calls
 * until such time as external calls can
 * use this timing facility. Current problem
 * is that external calls are doing their
 * own catching of sigalarms that should
 * be ours and we end up hung.
 * ---------------------------------------
 */
void	check_for_timer_pops()
{
	static boolean_t	stolen_timer = FALSE;	/* only complain once */
	int		stolenwhen = 0;		/* 0 = no, 1 = not first, 2 = first time */
	sigset_t 	savemask;
	struct sigaction current_sa;
	static char *whenstolen[] = {"check_for_timer_pops",
				     "check_for_timer_pops first time"};

	sigaction(SIGALRM, NULL, &current_sa);	/* get current info */
	if (!first_timeset)
	{	/* check if what we expected */
		if (timer_handler != current_sa.sa_handler)
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
		if ((SIG_IGN != current_sa.sa_handler) && /* as set by sig_init */
		    (SIG_DFL != current_sa.sa_handler))  /* utils, compile */
		{
			if (!stolen_timer)
			{
				stolen_timer = TRUE;
				stolenwhen = 2;
			}
		}
	}
	if (timeroot)
	{
		/* block SIGALRM signal */
		sigprocmask(SIG_BLOCK, &blockalrm, &savemask);

		timer_handler(DUMMY_SIG_NUM);

		/* reset signal handlers */
		sigprocmask(SIG_SETMASK, &savemask, NULL);
	}
	if (stolenwhen)
	{
		send_msg(VARLSTCNT(5) ERR_TIMERHANDLER, 3, current_sa.sa_handler,
			LEN_AND_STR(whenstolen[stolenwhen - 1]));
		rts_error(VARLSTCNT(5) ERR_TIMERHANDLER, 3, current_sa.sa_handler,
			LEN_AND_STR(whenstolen[stolenwhen - 1]));
		assert(FALSE);	/* does not return here */
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
