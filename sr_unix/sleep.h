/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SLEEP_H
#define SLEEP_H

/* Note: GT.M code *MUST NOT* use the sleep function because it causes problems with GT.M's timers on some platforms. Specifically,
 * the sleep function results in SIGARLM handler being silently deleted on Solaris systems (through Solaris 9 at least). This leads
 * to lost timer pops and has the potential for system hangs. The proper long sleep mechanism is hiber_start which can be accessed
 * through the LONG_SLEEP macro defined in mdef.h.
 *
 * On Linux boxes be sure to define USER_HZ macro (in gt_timers.c) appropriately to mitigate the timer clustering imposed by
 * the OS. Historically, the USER_HZ value has defaulted to 100 (same as HZ), thus resulting in at most 10ms accuracy when
 * delivering timed events.
 */

void m_usleep(int useconds);

#if !defined(_AIX) && !defined(__osf__) && !defined(__sparc) && !defined(_UWIN) && !defined(__linux__)
#   if !defined(__MVS__) && !defined(__CYGWIN__) && !defined(__hpux)
#      error "Unsure of support for sleep functions on this platform"
#   endif
#endif

#if defined(__MVS__) || defined(__CYGWIN__) || defined(__hpux) || defined(_AIX)
/* For HP-UX the clock_* seem to be missing; for AIX the accuracy of clock_* is currently poor */
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

#if defined(__MVS__) || defined(__CYGWIN__)
/* On z/OS neither clock_nanosleep nor nanosleep is available, so use a combination of sleep, usleep, and gettimeofday instead.
 * Since we do not have a z/OS box presently, this implementation has not been tested, and so it likely needs some casts at the very
 * least. Another note is that sleep is unsafe to mix with timers on other platforms, but on z/OS the documentation does not mention
 * any fallouts, so this should be verified. If it turns out that sleep is unsafe, we might have to use pthread_cond_timewait or
 * call usleep (which, given that we have used it on z/OS before, should be safe) in a loop.
 * Due to the above stated limitations the minimum sleep on z/OS is 1 Usec
 * cywin is a mystery so assume the worst */
#define SLEEP_USEC(MICROSECONDS, RESTART)						\
MBSTART {										\
	int 		secs, interrupted;						\
	useconds_t	usecs;								\
	struct timeval	now, expir;							\
											\
	assert(0 < MICROSECONDS);							\
	secs = (MICROSECONDS) / 1000;							\
	usecs = (MICROSECONDS) % E_6;							\
	SET_EXPIR_TIME(now, expir, secs, usecs);					\
	if (RESTART)									\
	{										\
		while (0 < secs)							\
		{									\
			sleep(secs);		/* BYPASSOK */				\
			/* This macro will break the loop when it is time. */		\
			UPDATE_REM_TIME_OR_BREAK(now, expir, secs, usecs);		\
		}									\
		while (0 < usecs)							\
		{									\
			usleep(usecs);							\
			/* This macro will break the loop when it is time. */		\
			UPDATE_REM_TIME_OR_BREAK(now, expir, secs, usecs);		\
		}									\
	} else										\
	{										\
		interrupted = sleep(secs);	/* BYPASSOK */				\
		if (!interrupted)							\
		{	/* This macro will break the loop when it is time. */		\
			UPDATE_REM_TIME_OR_BREAK(now, expir, secs, usecs);		\
			usleep(usecs);							\
		}									\
	}										\
} MBEND
#else

/* For most UNIX platforms a combination of nanosleep() and gettimeofday() proved to be the most supported, accurate, and
 * operationally sound approach. Alternatives for implementing high-resolution sleeps include clock_nanosleep() and nsleep()
 */
#  define SLEEP_USEC(MICROSECONDS, RESTART)						\
MBSTART {										\
	int 		status, usecs;							\
	struct timespec	req;								\
	struct timeval	now, expir;							\
											\
	assert(0 < MICROSECONDS);							\
	req.tv_sec = (time_t)((MICROSECONDS) / E_6);					\
	req.tv_nsec = (long)((usecs = (MICROSECONDS) % E_6) * 1000); /* Assignment! */	\
	assert(E_9 > req.tv_nsec);							\
	if (RESTART)									\
	{										\
		SET_EXPIR_TIME(now, expir, req.tv_sec, usecs);				\
		while ((-1 == (status = nanosleep(&req, NULL))) && (EINTR == errno))	\
		{	/* This macro will break the loop when it is time. */		\
			UPDATE_REM_TIME_OR_BREAK(now, expir, req.tv_sec, usecs);	\
			req.tv_nsec = (long)(usecs * 1000);				\
		}									\
	} else										\
		nanosleep(&req, NULL);							\
} MBEND
#endif

# define NANOSLEEP(NANOSECONDS, RESTART)						\
MBSTART {										\
	SLEEP_USEC((1000 > (NANOSECONDS)) ? 1 : ((NANOSECONDS) / 1000), RESTART);	\
} MBEND
#endif
#if !defined(__MVS__) && !defined(__CYGWIN__) && !defined(__hpux)
/* Nonetheless, because we continue to press for the highest time discrimination available, where posible we use
 * clock_nanosleep and clock_gettime, which, while currently no faster than gettimeofday(), do eventually promise
 * sub-millisecond accuracy
 *
 * Because, as of this writing, in AIX the clock_* routines are so erratic with short times we use the functions above for most
 * things but give the following macro a separate name so AIX can use it in op_hang.c to ensure that a 1 second sleep always
 * puts the process in a different second as measured by $HOROLOG and the like.
 */
# define CLOCK_NANOSLEEP(NANOSECONDS, RESTART)							\
MBSTART {											\
	int 		STATUS;									\
	struct timespec	REQTIM;									\
												\
	assert(0 < (NANOSECONDS));								\
	clock_gettime(CLOCK_MONOTONIC, &REQTIM);						\
	REQTIM.tv_nsec += (long)(NANOSECONDS);							\
	if (E_9 <= REQTIM.tv_nsec)								\
	{											\
		REQTIM.tv_sec += (time_t)(REQTIM.tv_nsec / E_9);				\
		REQTIM.tv_nsec %= E_9;								\
	}											\
	do											\
	{											\
		STATUS = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &REQTIM, NULL);	\
		if (!RESTART || (0 == STATUS))							\
			break;									\
		assertpro (EINTR == STATUS);							\
	} while (TRUE);										\
} MBEND

#if !defined(_AIX)
# define SLEEP_USEC(MICROSECONDS, RESTART)						\
MBSTART {										\
	NANOSLEEP(((MICROSECONDS) * 1000), RESTART);					\
} MBEND

# define NANOSLEEP(NANOSECONDS, RESTART)						\
MBSTART {										\
	CLOCK_NANOSLEEP(NANOSECONDS, RESTART);					\
} MBEND
#endif
#endif
#endif /* SLEEP_H */
