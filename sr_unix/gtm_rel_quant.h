/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_REL_QUANT_INCLUDED
#define GTM_REL_QUANT_INCLUDED

#include <sys/time.h>
#include <errno.h>
#include <sched.h>
#include <time.h>
#include "gtm_unistd.h"
#include "sleep.h"

GBLREF	uint4		process_id;
/* yield processor macro - if argument is 0 or the (pseudo-)random value whose limit the argument defines is 0 just yield
 * otherwise do a microsleep
 */
#if defined(_AIX)
/* For pSeries the "yield" system call seems a better match for
 * yields to ALLprocesses instead of just those on the local processor queue.
 */
void yield(void);		/* AIX doesn't have this in a header, so use prototype from the man page. */
#define RELQUANT yield()
#else
#define RELQUANT sched_yield()	/* avoiding pthread_yield() avoids unnecessary linking with libpthreads */
#endif

#define	USEC_IN_NSEC_MASK		0x3FF

#define	GTM_REL_QUANT(MAX_TIME_MASK)									\
MBSTART {												\
	int		NANO_SLEEP_TIME;								\
	static uint4	TIME_ADJ = 0;									\
													\
	/* process_id provides cheap pseudo-random across processes, but add in a timestamp/counter	\
	 * so that the number varies a bit.								\
	 */												\
	if (0 == TIME_ADJ)										\
		TIME_ADJ = (uint4) time(NULL);								\
	if (MAX_TIME_MASK)										\
	{	/* To get a value that moves a bit, xor with a timestamp/counter. */			\
		NANO_SLEEP_TIME = (process_id ^ (TIME_ADJ++)) & (MAX_TIME_MASK);			\
		if (!NANO_SLEEP_TIME)									\
			NANO_SLEEP_TIME = MAX_TIME_MASK;						\
		assert((NANO_SLEEP_TIME < E_9) && (NANO_SLEEP_TIME > 0));				\
		NANOSLEEP(NANO_SLEEP_TIME, FALSE);							\
	} else												\
		RELQUANT;										\
} MBEND

/* Sleep/rel_quant <= 1 micro-second every 4 iterations and also perform caslatch check every ~4 seconds */
#define	REST_FOR_LATCH(LATCH, MAX_SLEEP_MASK, RETRIES)									\
MBSTART {														\
	if (0 == (RETRIES & LOCK_SPIN_HARD_MASK))	/* On every so many passes, sleep rather than spinning */	\
	{														\
		GTM_REL_QUANT((MAX_SLEEP_MASK));	/* Release processor to holder of lock (hopefully) */		\
		/* Check if we're due to check for lock abandonment check or holder wakeup */				\
		if (0 == (RETRIES & (LOCK_CASLATCH_CHKINTVL_USEC - 1)))							\
			performCASLatchCheck(LATCH, TRUE);								\
	}														\
} MBEND

/* Sleep 1 micro-second every 4 iterations and also perform caslatch check every ~4 seconds */
#define	SLEEP_FOR_LATCH(LATCH, RETRIES)										\
MBSTART {														\
	if (0 == (RETRIES & LOCK_SPIN_HARD_MASK))	/* On every so many passes, sleep rather than spinning */	\
	{														\
		SLEEP_USEC(1, FALSE);	/* Release processor to holder of lock (hopefully) */				\
		/* Check if we're due to check for lock abandonment check or holder wakeup */				\
		if (0 == (RETRIES & (LOCK_CASLATCH_CHKINTVL_USEC - 1)))							\
			performCASLatchCheck(LATCH, TRUE);								\
	}														\
} MBEND

#endif /* GTM_REL_QUANT_INCLUDED */
