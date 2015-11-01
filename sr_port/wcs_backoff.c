/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "sleep_cnt.h"

#include <stdlib.h>
#include "gt_timer.h"
#ifdef UNIX
#include "random.h"
#endif
#include "wcs_backoff.h"

GBLREF int4 process_id;

void wcs_backoff(unsigned int sleepfactor)
{
	/* wcs_backoff provides a layer over hiber_start that produces a [pseudo] random sleep varying
	 * to a maximum sleep time it is intended to be used in as part of a contention backoff
	 * where the argument is the attempt count. If the counter starts at 0, the invocation would
	 * typically be:
	 *   if (count) wcs_backoff(count);
	 */

#if defined(VMS)
	float           mth$random();
	uint4           lib$day();
	int4            day;
#endif

	static int4	seed = 0;
	uint4		sleep_ms;

	assert(sleepfactor);
	if (0 == sleepfactor)
		return;
	if (sleepfactor > MAXSLPTIME)
		sleepfactor = MAXSLPTIME;

#ifdef UNIX
	if (seed == 0)
	{
		init_rand_table();
		seed = 1;
	}
	sleep_ms = ((uint4)(get_rand_from_table() % sleepfactor));
#else
	if (seed == 0)				/* Seed random number generator */
		lib$day(&day, 0, &seed);
	sleep_ms = ((uint4)(sleepfactor * mth$random(&seed)));
#endif
	if (0 == sleep_ms)
		return;				/* We have no wait this time */
	if (1000 > sleep_ms)			/* Use simpler sleep for shorties */
	{
		SHORT_SLEEP(sleep_ms);
	}
	else
		hiber_start(sleep_ms);		/* Longer sleeps use brute force */
	return;
}
