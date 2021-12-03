/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SLEEP_H
#define SLEEP_H

#include "have_crit.h"

#define	RESTART_FALSE	FALSE
#define	RESTART_TRUE	TRUE

/* Note: GT.M code *MUST NOT* use the sleep function because it causes problems with GT.M's timers on some platforms. Specifically,
 * the sleep function results in SIGARLM handler being silently deleted on Solaris systems (through Solaris 9 at least). This leads
 * to lost timer pops and has the potential for system hangs. The proper long sleep mechanism is hiber_start which can be accessed
 * through the LONG_SLEEP macro defined in mdef.h.
 *
 * On Linux boxes be sure to define USER_HZ macro (in gt_timers.c) appropriately to mitigate the timer clustering imposed by
 * the OS. Historically, the USER_HZ value has defaulted to 100 (same as HZ), thus resulting in at most 10ms accuracy when
 * delivering timed events.
 */

/* SLEEP_USEC wrapper, see sleep.c for more information */
void m_usleep(int useconds);

<<<<<<< HEAD
#if !defined(__linux__)
#      error "Unsure of support for sleep functions on this platform"
#endif

#define	MT_SAFE_TRUE	TRUE
#define	MT_SAFE_FALSE	FALSE

/* Nonetheless, because we continue to press for the highest time discrimination available, where posible we use
 * clock_nanosleep and clock_gettime, which, while currently no faster than gettimeofday(), do eventually promise
 * sub-millisecond accuracy
 *
 * Because, as of this writing, in AIX the clock_* routines are so erratic with short times we use the functions above for most
 * things but give the following macro a separate name so AIX can use it in op_hang.c to ensure that a 1 second sleep always
 * puts the process in a different second as measured by $HOROLOG and the like.
 */
# define CLOCK_NANOSLEEP(NANOSECONDS, RESTART, MT_SAFE)						\
MBSTART {											\
	int 		STATUS;									\
	struct timespec	REQTIM;									\
												\
	assert(0 < (NANOSECONDS));								\
	clock_gettime(CLOCK_MONOTONIC, &REQTIM);						\
	if (NANOSECS_IN_SEC <= (NANOSECONDS) + REQTIM.tv_nsec)					\
	{											\
		REQTIM.tv_sec += (time_t)(((NANOSECONDS) + REQTIM.tv_nsec) / NANOSECS_IN_SEC);	\
		REQTIM.tv_nsec = ((NANOSECONDS) + REQTIM.tv_nsec) % NANOSECS_IN_SEC;		\
	}											\
	else											\
		REQTIM.tv_nsec += (long)(NANOSECONDS);						\
	do											\
	{											\
		STATUS = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &REQTIM, NULL);	\
		assertpro((0 == STATUS) || (EINTR == STATUS));					\
		/* If MT_SAFE is FALSE the caller is in a multi-threaded environment and does	\
		 * not hold the YottaDB engine lock. Therefore skip invoking macros which	\
		 * assume a single-threaded engine execution.					\
		 */										\
		if (!(RESTART) || (0 == STATUS))						\
		{										\
			if (MT_SAFE)								\
			{	/* Check if any signal handling got deferred during the sleep.	\
				 * If so handle it safely outside of the signal handler.	\
				 * No need of "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" call because	\
				 * the below does the same thing.				\
				 */								\
				DEFERRED_SIGNAL_HANDLING_CHECK;					\
			}									\
			break;									\
		}										\
		if (MT_SAFE)									\
			eintr_handling_check();							\
	} while (TRUE);										\
} MBEND

# define SLEEP_USEC(MICROSECONDS, RESTART)						\
MBSTART {										\
	/* With an 8-byte input MICROSECONDS variable, we can represent			\
	 * a sleep time corresponding to billions of seconds even when it is		\
	 * converted into nanoseconds. But with a 4-byte input, we can only		\
	 * represent around 4 seconds of sleep time. Hence allow a sleep time		\
	 * of > 1 second only if MICROSECONDS is 8-byte else allow < 1 second.		\
	 */										\
	assert((8 == SIZEOF(MICROSECONDS)) ||						\
			((MICROSECS_IN_SEC > MICROSECONDS) && (0 < MICROSECONDS)));	\
	NANOSLEEP(((MICROSECONDS) * NANOSECS_IN_USEC), RESTART);			\
=======
#if !defined(_AIX) && !defined(__linux__) && !defined(__MVS__) && !defined(__CYGWIN__)
# error "Unsure of support for sleep functions on this platform"
#endif

/* Where possible we use clock_nanosleep and clock_gettime, which, while currently no faster than gettimeofday(), do eventually
 * promise sub-millisecond accuracy.
 */
#define CLOCK_NANOSLEEP(CLOCKID, SECONDS, NANOSECONDS, RESTART)					\
MBSTART {											\
	int 		STATUS;									\
	struct timespec	REQTIM;									\
												\
	assert(0 <= (SECONDS));									\
	assert((0 <= (NANOSECONDS)) && (E_9 > (NANOSECONDS)));					\
	clock_gettime(CLOCKID, &REQTIM);							\
	REQTIM.tv_sec += (long)(SECONDS);							\
	REQTIM.tv_nsec += (long)(NANOSECONDS);							\
	if (NANOSECS_IN_SEC <= REQTIM.tv_nsec)							\
	{											\
		REQTIM.tv_sec++;								\
		REQTIM.tv_nsec -= NANOSECS_IN_SEC;						\
	}											\
	do											\
	{											\
		STATUS = clock_nanosleep(CLOCKID, TIMER_ABSTIME, &REQTIM, NULL);		\
		if (!RESTART || (0 == STATUS))							\
			break;									\
		assert(EINTR == STATUS);							\
	} while (TRUE);										\
} MBEND

/* For most UNIX platforms a combination of nanosleep() and gettimeofday() proved to be the most supported, accurate, and
 * operationally sound approach. Alternatives for implementing high-resolution sleeps include clock_nanosleep() and nsleep()
 */
#define SET_EXPIR_TIME(NOW_TIMEVAL, EXPIR_TIMEVAL, SECS, USECS)				\
MBSTART {										\
	gettimeofday(&(NOW_TIMEVAL), NULL);						\
	if (E_6 <= ((NOW_TIMEVAL).tv_usec + USECS))					\
	{										\
		(EXPIR_TIMEVAL).tv_sec = (NOW_TIMEVAL).tv_sec + (SECS) + 1;		\
		(EXPIR_TIMEVAL).tv_usec = (NOW_TIMEVAL).tv_usec + (USECS) - E_6;	\
	} else										\
	{										\
		(EXPIR_TIMEVAL).tv_sec = (NOW_TIMEVAL).tv_sec + (SECS);			\
		(EXPIR_TIMEVAL).tv_usec = (NOW_TIMEVAL).tv_usec + (USECS);		\
	}										\
} MBEND

/* This macro *does not* have surrounding braces and *will break* out of the block it is in on non-positive remaining time. */
#define UPDATE_REM_TIME_OR_BREAK(NOW_TIMEVAL, EXPIR_TIMEVAL, SECS, USECS)		\
	gettimeofday(&(NOW_TIMEVAL), NULL);						\
	if (((NOW_TIMEVAL).tv_sec > (EXPIR_TIMEVAL).tv_sec)				\
		|| (((NOW_TIMEVAL).tv_sec == (EXPIR_TIMEVAL).tv_sec)			\
			&& ((NOW_TIMEVAL).tv_usec >= (EXPIR_TIMEVAL).tv_usec)))		\
		break;									\
	if ((EXPIR_TIMEVAL).tv_usec < (NOW_TIMEVAL).tv_usec)				\
	{										\
		SECS = (time_t)((EXPIR_TIMEVAL).tv_sec - (NOW_TIMEVAL).tv_sec - 1);	\
		USECS = (int)(E_6 + (EXPIR_TIMEVAL).tv_usec - (NOW_TIMEVAL).tv_usec);	\
	} else										\
	{										\
		SECS = (time_t)((EXPIR_TIMEVAL).tv_sec - (NOW_TIMEVAL).tv_sec);		\
		USECS = (int)((EXPIR_TIMEVAL).tv_usec - (NOW_TIMEVAL).tv_usec);		\
	}										\

/* Because, as of this writing, in AIX the clock_* routines are so erratic with short times we use the functions mentioned above
 * for most things but give the following macro a separate name so AIX can use it in op_hang.c to ensure that a 1 second sleep
 * always puts the process in a different second as measured by $HOROLOG and the like.
 */
#define MICROSECOND_SLEEP_with_NANOSECOND_SLEEP(USECONDS, RESTART)			\
MBSTART {										\
	int 		status, usecs, save_errno;					\
	struct timespec	req;								\
	struct timeval	now, expir;							\
											\
	assert(0 < (USECONDS));								\
	req.tv_sec = (time_t)((USECONDS) / E_6);					\
	req.tv_nsec = (long)((usecs = (USECONDS) % E_6) * 1000); /* Assignment! */	\
	assert(E_9 > req.tv_nsec);							\
	/* A little wasteful for the non-restart case */				\
	SET_EXPIR_TIME(now, expir, req.tv_sec, usecs);					\
	do										\
	{	/* This macro will break the loop when it is time. */			\
		status = nanosleep(&req, NULL);						\
		if (!(RESTART) || (0 == status))					\
			break;								\
		UPDATE_REM_TIME_OR_BREAK(now, expir, req.tv_sec, usecs);		\
		req.tv_nsec = (long)(usecs * 1000);					\
		assert(EINTR == (save_errno = errno)); /* inline assignment */		\
	} while (TRUE);									\
} MBEND

/* On z/OS neither clock_nanosleep nor nanosleep is available, so use a combination of sleep, usleep, and gettimeofday instead.
 * Since we do not have a z/OS box presently, this implementation has not been tested, and so it likely needs some casts at the very
 * least. Another note is that sleep is unsafe to mix with timers on other platforms, but on z/OS the documentation does not mention
 * any fallouts, so this should be verified. If it turns out that sleep is unsafe, we might have to use pthread_cond_timewait or
 * call usleep (which, given that we have used it on z/OS before, should be safe) in a loop.
 * Due to the above stated limitations the minimum sleep on z/OS is 1 Usec
 * cywin is a mystery so assume the worst */
#define MICROSECOND_SLEEP_with_SLEEP_and_USLEEP(USECONDS, RESTART)			\
MBSTART {										\
	int 		secs, interrupted;						\
	useconds_t	usecs;								\
	struct timeval	now, expir;							\
											\
	assert(0 < (USECONDS));								\
	secs = (USECONDS) / E_6;							\
	usecs = (USECONDS) % E_6;							\
	SET_EXPIR_TIME(now, expir, secs, usecs);					\
	do										\
	{										\
		/* Sleep for seconds first */						\
		interrupted = sleep(secs);	/* BYPASSOK */				\
		if (interrupted && !(RESTART))						\
			break;								\
		/* This macro will break the loop when it is time. */			\
		UPDATE_REM_TIME_OR_BREAK(now, expir, secs, usecs);			\
		/* Recalculate time and sleep for remaining microseconds */		\
		interrupted = usleep(usecs);	/* BYPASSOK */				\
		if (interrupted && !(RESTART))						\
			break;								\
		/* This macro will break the loop when it is time. */			\
		UPDATE_REM_TIME_OR_BREAK(now, expir, secs, usecs);			\
	} while ((0 < secs) || (0 < usecs));						\
} MBEND

#if defined(__MVS__) || defined(__CYGWIN__)
/* z/OS and Cygwin use the lowest quality sleep options. Sub-millisecond nanosecond sleeps round up to 1 millisecond */
# define SLEEP_USEC(USECONDS, RESTART)		MICROSECOND_SLEEP_with_SLEEP_and_USLEEP(USECONDS, RESTART)
# define NANOSLEEP(NANOSECONDS, RESTART)	SLEEP_USEC((1000 > (NANOSECONDS)) ? 1 : ((NANOSECONDS) / 1000), RESTART);
#elif defined(_AIX)
/* Because of unreliability, AIX uses plain old nanosleep(). Sub-millisecond nanosecond sleeps round up to 1 millisecond */
# define SLEEP_USEC(USECONDS, RESTART)		MICROSECOND_SLEEP_with_NANOSECOND_SLEEP(USECONDS, RESTART)
# define NANOSLEEP(NANOSECONDS, RESTART)	SLEEP_USEC((1000 > (NANOSECONDS)) ? 1 : ((NANOSECONDS) / 1000), RESTART);
#else
/* All other platforms use high performance sleeps with CLOCK_NANOSLEEP */
# define SLEEP_USEC(USECONDS, RESTART)							\
MBSTART {										\
	time_t	seconds = (USECONDS) / E_6;						\
	time_t	nanoseconds = (USECONDS % E_6) * NANOSECS_IN_USEC;			\
											\
	CLOCK_NANOSLEEP(CLOCK_MONOTONIC, seconds, nanoseconds, RESTART);		\
>>>>>>> 52a92dfd (GT.M V7.0-001)
} MBEND

# define NANOSLEEP(NANOSECONDS, RESTART)						\
MBSTART {										\
<<<<<<< HEAD
	assert((8 == SIZEOF(NANOSECONDS)) ||						\
			((NANOSECS_IN_SEC > NANOSECONDS) && (0 < NANOSECONDS)));	\
	CLOCK_NANOSLEEP(NANOSECONDS, RESTART, MT_SAFE_TRUE);				\
} MBEND

# define SLEEP_USEC_MULTI_THREAD_UNSAFE(MICROSECONDS, RESTART)				\
MBSTART {										\
	assert((8 == SIZEOF(MICROSECONDS)) ||						\
			((MICROSECS_IN_SEC > MICROSECONDS) && (0 < MICROSECONDS)));	\
	CLOCK_NANOSLEEP(((MICROSECONDS) * NANOSECS_IN_USEC), RESTART, MT_SAFE_FALSE);	\
} MBEND

=======
	time_t	seconds = (NANOSECONDS) / E_9;						\
	time_t	nanoseconds = (NANOSECONDS % E_9);					\
											\
	/* Really shouldn't be using this macro for sleep > 1 second */			\
	assert(E_9 >= (NANOSECONDS));							\
	CLOCK_NANOSLEEP(CLOCK_MONOTONIC, seconds, nanoseconds, RESTART);		\
} MBEND
#endif
>>>>>>> 52a92dfd (GT.M V7.0-001)
#endif /* SLEEP_H */
