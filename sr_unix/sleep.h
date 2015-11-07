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

#ifndef SLEEP_H
#define SLEEP_H

/* Note: GT.M code *MUST NOT* use the sleep function because it causes problems with GT.M's timers on some platforms. Specifically,
 * the sleep function results in SIGARLM handler being silently deleted on Solaris systems (through Solaris 9 at least). This leads
 * to lost timer pops and has the potential for system hangs. The proper long sleep mechanism is hiber_start which can be accessed
 * through the LONG_SLEEP macro defined in mdef.h.
 */

int m_sleep(int seconds);
int m_usleep(int useconds);
int m_nsleep(int nseconds);

#ifdef UNIX

# if !defined(_AIX) && !defined(__osf__) && !defined(__hpux) && !defined(__sparc) && !defined(_UWIN) && !defined(__linux__)
#   if !defined(__MVS__) && !defined(__CYGWIN__)
#      error "Unsure of support for sleep functions on this platform"
#   endif
# endif

#define E_6	1000000
#define E_9	1000000000

#define SET_EXPIR_TIME(NOW_TIMEVAL, EXPIR_TIMEVAL, SECS, USECS)						\
{													\
	gettimeofday(&(NOW_TIMEVAL), NULL);								\
	if (E_6 <= ((NOW_TIMEVAL).tv_usec + USECS))							\
	{												\
		(EXPIR_TIMEVAL).tv_sec = (NOW_TIMEVAL).tv_sec + (SECS) + 1;				\
		(EXPIR_TIMEVAL).tv_usec = (NOW_TIMEVAL).tv_usec + (USECS) - E_6;			\
	} else												\
	{												\
		(EXPIR_TIMEVAL).tv_sec = (NOW_TIMEVAL).tv_sec + (SECS);					\
		(EXPIR_TIMEVAL).tv_usec = (NOW_TIMEVAL).tv_usec + (USECS);				\
	}												\
}

#define EVAL_REM_TIME(NOW_TIMEVAL, EXPIR_TIMEVAL, SECS, USECS)						\
{													\
	gettimeofday(&(NOW_TIMEVAL), NULL);								\
	if (((NOW_TIMEVAL).tv_sec > (EXPIR_TIMEVAL).tv_sec)						\
		|| (((NOW_TIMEVAL).tv_sec == (EXPIR_TIMEVAL).tv_sec)					\
			&& ((NOW_TIMEVAL).tv_usec >= (EXPIR_TIMEVAL).tv_usec)))				\
		return;											\
	if ((EXPIR_TIMEVAL).tv_usec < (NOW_TIMEVAL).tv_usec)						\
	{												\
		SECS = (time_t)((EXPIR_TIMEVAL).tv_sec - (NOW_TIMEVAL).tv_sec - 1);			\
		USECS = (int)(E_6 + (EXPIR_TIMEVAL).tv_usec - (NOW_TIMEVAL).tv_usec);			\
	} else												\
	{												\
		SECS = (time_t)((EXPIR_TIMEVAL).tv_sec - (NOW_TIMEVAL).tv_sec);				\
		USECS = (int)((EXPIR_TIMEVAL).tv_usec - (NOW_TIMEVAL).tv_usec);				\
	}												\
}

#if defined(__osf__) || defined(__hpux)
/* The above platforms do not support clock_nanosleep, so use nanosleep. To avoid sleeping for much
 * longer than requested in case of pathologically many interrupts, recalculate the remaining duration
 * with gettimeofday.
 */
#  define NANOSLEEP(MS)											\
{													\
	int 		status, usecs;									\
	struct timespec	req;										\
	struct timeval	now, expir;									\
													\
	assert(0 < MS);											\
	req.tv_sec = (time_t)(MS / 1000);								\
	usecs = (MS % 1000) * 1000;									\
	req.tv_nsec = (long)(usecs * 1000);								\
	assert(E_9 > req.tv_nsec);									\
	SET_EXPIR_TIME(now, expir, req.tv_sec, usecs)							\
	while ((-1 == (status = nanosleep(&req, NULL))) && (EINTR == errno))				\
	{												\
		EVAL_REM_TIME(now, expir, req.tv_sec, usecs);						\
		req.tv_nsec = (long)(usecs * 1000);							\
	}												\
}
#elif defined(__MVS__)
/* On z/OS neither clock_nanosleep nor nanosleep is available, so use a combination of sleep, usleep,
 * and gettimeofday instead. Since we do not have a z/OS box presently, this implementation has not
 * been tested, and so it likely needs some casts at the very least. Another note is that sleep is
 * unsafe to mix with timers on other platforms, but on z/OS the documentation does not mention any
 * fallouts, so this should be verified. If it turns out that sleep is unsafe, we might have to use
 * pthread_cond_timewait or call usleep (which, given that we have used it on z/OS before, should be
 * safe) in a loop.
 */
#  define NANOSLEEP(MS)											\
{													\
	int 		secs;										\
	useconds_t	usecs;										\
	struct timeval	now, expir;									\
													\
	assert(0 < MS);											\
	secs = MS / 1000;										\
	usecs = MS % 1000;										\
	SET_EXPIR_TIME(now, expir, secs, usecs)								\
	while (0 < secs)										\
	{												\
		sleep(secs);	/* BYPASSOK:  */							\
		EVAL_REM_TIME(now, expir, secs, usecs);							\
	}												\
	while (0 < usecs)										\
	{												\
		usleep(usecs);										\
		EVAL_REM_TIME(now, expir, secs, usecs);							\
	}												\
}
#else
/* The major supported platforms should have clock_nanosleep implementation, so to avoid extending the
 * actual sleep times, we use clock_gettime to first obtain the point of reference, and then specify
 * the TIMER_ABSTIME flag when invoking clock_nanosleep for absolute offsets. In case CLOCK_REALTIME
 * type of clock is not supported on some platform, we fall back on nanosleep.
 */
#  define NANOSLEEP(MS)											\
{													\
	int 		status, usecs;									\
	struct timespec	req, cur;									\
	struct timeval	now, expir;									\
													\
	req.tv_sec = (time_t)(MS / 1000);								\
	usecs = (MS % 1000) * 1000;									\
	req.tv_nsec = (long)(usecs * 1000);								\
	assert(E_9 > req.tv_nsec);									\
	if ((-1 == (status = clock_gettime(CLOCK_REALTIME, &cur))) && (EINVAL == errno))		\
	{												\
		SET_EXPIR_TIME(now, expir, req.tv_sec, usecs)						\
		while ((-1 == (status = nanosleep(&req, NULL))) && (EINTR == errno))			\
		{											\
			EVAL_REM_TIME(now, expir, req.tv_sec, usecs);					\
			req.tv_nsec = (long)(usecs * 1000);						\
		}											\
	} else												\
	{												\
		if (E_9 <= cur.tv_nsec + req.tv_nsec)							\
		{											\
			req.tv_sec += (cur.tv_sec + 1);							\
			req.tv_nsec = cur.tv_nsec + req.tv_nsec - E_9;					\
		} else											\
		{											\
			req.tv_sec += cur.tv_sec;							\
			req.tv_nsec += cur.tv_nsec;							\
		}											\
		while ((-1 == (status = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &req, NULL)))	\
			&& (EINTR == errno));								\
	}												\
}
#endif

# ifdef _AIX
   typedef struct timestruc_t m_time_t;
#  define nanosleep_func nsleep
# endif

# if defined(__sparc) || defined(__hpux) || defined(__osf__) || defined (__linux__) || defined (__CYGWIN__)
   typedef struct timespec m_time_t;
#  define nanosleep_func nanosleep
# endif

# ifdef _UWIN
# include "iotcp_select.h"
# define usleep_func gtm_usleep
# endif

# ifdef __MVS__
   typedef struct timespec m_time_t;
#  define nanosleep_func usleep			/* m_nsleep will not work on OS390, but it is not used */
# endif

#else

# error  "Unsure of support for sleep functions on this non-UNIX platform"

#endif

#endif /* SLEEP_H */
