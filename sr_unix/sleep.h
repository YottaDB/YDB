/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
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

#if !defined(__linux__)
#      error "Unsure of support for sleep functions on this platform"
#endif

#define	MT_SAFE_TRUE	TRUE
#define	MT_SAFE_FALSE	FALSE

/* Where possible we use clock_nanosleep and clock_gettime, which, while currently no faster than gettimeofday(), do eventually
 * promise sub-millisecond accuracy.
 */
# define CLOCK_NANOSLEEP(CLOCKID, SECONDS, NANOSECONDS, RESTART, MT_SAFE)			\
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
	NANOSLEEP((MICROSECONDS) * NANOSECS_IN_USEC, RESTART, MT_SAFE_TRUE);		\
} MBEND

# define NANOSLEEP(NANOSECONDS, RESTART, MT_SAFE)					\
MBSTART {										\
	time_t	tv_sec;									\
	long	tv_nsec;								\
											\
	/* With an 8-byte input MICROSECONDS variable, we can represent			\
	 * a sleep time corresponding to billions of seconds even when it is		\
	 * converted into nanoseconds. But with a 4-byte input, we can only		\
	 * represent around 4 seconds of sleep time. Hence allow a sleep time		\
	 * of > 1 second only if MICROSECONDS is 8-byte else allow < 1 second.		\
	 */										\
	assert((8 == SIZEOF(NANOSECONDS)) ||						\
			((NANOSECS_IN_SEC > NANOSECONDS) && (0 < NANOSECONDS)));	\
	tv_sec = NANOSECONDS / NANOSECS_IN_SEC;						\
	tv_nsec = (NANOSECONDS - (tv_sec * NANOSECS_IN_SEC));				\
	CLOCK_NANOSLEEP(CLOCK_MONOTONIC, tv_sec, tv_nsec, RESTART, MT_SAFE);		\
} MBEND

# define SLEEP_USEC_MULTI_THREAD_UNSAFE(MICROSECONDS, RESTART)				\
MBSTART {										\
	NANOSLEEP(((MICROSECONDS) * NANOSECS_IN_USEC), RESTART, MT_SAFE_FALSE);		\
} MBEND

#endif /* SLEEP_H */
