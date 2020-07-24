/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
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

void m_usleep(int useconds);

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
} MBEND

# define NANOSLEEP(NANOSECONDS, RESTART)						\
MBSTART {										\
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

#endif /* SLEEP_H */
