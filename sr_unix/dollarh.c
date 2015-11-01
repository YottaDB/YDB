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

#include <time.h>
#include <sys/time.h>
#include "dollarh.h"

#define MINUTE	60
#define HOUR	MINUTE*60	/* one hour in seconds 60 * 60 */
#define DAYS	47117		/* correction for 1841 - 1970 */
#define ONEDAY	86400		/* seconds */

GBLREF bool	run_time;

/* note to code consolidaters: this code seems to run fine on VMS */
void dollarh(time_t intime, uint4 *days, time_t *seconds)
{
	uint4		tdays;
	struct tm	*ttime;
#ifdef DEBUG
	static uint4	old_days = 0, old_seconds = 0;
	static time_t	old_intime = 0;
#endif

	ttime = gmtime(&intime);		/* represent intime as UCT */
	ttime->tm_isdst = -1; 			/* force mktime to find out what current adjustments apply to UCT */
	tdays = (uint4)((intime + (time_t)difftime(intime, mktime(ttime))) / ONEDAY) + DAYS;	/* adjust relative to UTC */
	ttime = localtime(&intime);		/* represent intime as local time in case of offsets from UCT other than hourly */
	*seconds  = (time_t)(ttime->tm_hour * HOUR) + (ttime->tm_min * MINUTE) + ttime->tm_sec;
	*days = tdays;				/* use temp local in case the caller has overlapped arguments */
	assert(!run_time || ((*days == old_days) && (*seconds >= old_seconds)) || (*days > old_days) || (intime <= old_intime));
	DEBUG_ONLY(old_seconds = *seconds; old_days = *days; old_intime = intime;)
	return;
}
