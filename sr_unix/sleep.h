/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
 */

void m_usleep(int useconds);

# if !defined(_AIX) && !defined(__osf__) && !defined(__hpux) && !defined(__sparc) && !defined(_UWIN) && !defined(__linux__) && !defined(__APPLE__)
#   if !defined(__MVS__) && !defined(__CYGWIN__)
#      error "Unsure of support for sleep functions on this platform"
#   endif
# endif

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

#ifdef __MVS__
/* On z/OS neither clock_nanosleep nor nanosleep is available, so use a combination of sleep, usleep, and gettimeofday instead.
 * Since we do not have a z/OS box presently, this implementation has not been tested, and so it likely needs some casts at the very
 * least. Another note is that sleep is unsafe to mix with timers on other platforms, but on z/OS the documentation does not mention
 * any fallouts, so this should be verified. If it turns out that sleep is unsafe, we might have to use pthread_cond_timewait or
 * call usleep (which, given that we have used it on z/OS before, should be safe) in a loop.
 */
#  define SLEEP_USEC(MICROSECONDS, RESTART)						\
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
/* On most UNIX platforms a combination of nanosleep() and gettimeofday() proved to be the most supported, accurate, and
 * operationally sound approach. Alternatives for implementing high-resolution sleeps include clock_nanosleep() and nsleep()
 * (AIX only); however, neither of those provide better accuracy or speed. Additionally, the clock_gettime() function does not
 * prove to be any faster than gettimeofday(), and since we do not (yet) operate at sub-millisecond levels, it is not utilized.
 *
 * On Linux boxes be sure to define USER_HZ macro (in gt_timers.c) appropriately to mitigate the timer clustering imposed by
 * the OS. Historically, the USER_HZ value has defaulted to 100 (same as HZ), thus resulting in at most 10ms accuracy when
 * delivering timed events.
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

#endif /* SLEEP_H */
