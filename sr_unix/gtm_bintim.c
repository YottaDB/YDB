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

/* int gtm_bintim(char *s, int4 *timep)
 *
 * Converts an absolute or relative time to the UNIX internal format (seconds
 * past 1970)
 *
 * Input:
 *    s   	ASCII string containing an absolute or relative time
 *	  	specification (see below).
 *
 *    timep	pointer to a variable which will hold the absolute or
 *		relative time.  *timep's value should be interpreted
 *		as follows:
 *		   *timep >  0		absolute time
 *		   *timep <= 0		relative time
 *
 * Returns:  0  = success
 *	     -1 = failure
 *
 * ASCII time format:
 * dd-mmm-yyyy [hh:mm[:ss[:cc]]]	absolute time
 * -- hh:mm[:ss[:cc]]			absolute time (w/today's date)
 *
 * dd hh:mm[:ss[:cc]]			relative time
 *
 *
 */

#include "mdef.h"

#include <time.h>

#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_bintim.h"

static int getmon (char *month);

int gtm_bintim (char *s, int4 *timep)
{
    time_t	now;
    struct tm	time_tm, *now_tm;
    int		n, sec, min, hour, day, year;
    char	month[256];
    int		len = strlen(s),
		matched = 0;

    *month = '\0';
    now = time((time_t *) 0);
    now_tm = localtime(&now);

    n = SSCANF(s,"%d %d", &day, &hour);
    if (n == 2)  /* delta time format */
    {
	n = SSCANF(s, "%d %d:%d:%d%n", &day, &hour, &min, &sec, &matched);
	if (matched < len)
	{
	    n = SSCANF(s, "%d %d:%d:%d:%*d%n", &day, &hour, &min, &sec,
		       &matched);
	    if (matched < len)
	    {
		sec = 0;
		n = SSCANF(s, "%d %d:%d%n", &day, &hour, &min, &matched);
		if (matched < len)
		    return -1;
	    }
	}
	*timep = - (day*86400 + hour*3600 + min*60 + sec);
	return 0;
    }
    else	 /* absolute time format */
    {
	n = SSCANF(s, "%d-%[a-zA-Z]-%d %d:%d:%d%n", &day, month, &year,
		   &hour, &min, &sec, &matched);

	if (matched < len)
	{
	    n = SSCANF(s, "%d-%[a-zA-Z]-%d %d:%d:%d:%*d%n", &day, month,
		       &year, &hour, &min, &sec, &matched);

	    if (matched < len)
	    {
		sec = 0;
		n = SSCANF(s, "%d-%[a-zA-Z]-%d %d:%d%n", &day, month, &year,
			   &hour, &min, &matched);

		if (matched < len)
		{
		    hour = min = sec = 0;
		    n = SSCANF(s, "%d-%[a-zA-Z]-%d%n", &day, month, &year,
			       &matched);
		    if (matched < len)
		    {
			day = now_tm->tm_mday;
			year = now_tm->tm_year + 1900;
			n = SSCANF(s, "-- %d:%d:%d%n", &hour, &min, &sec,
				   &matched);
			if (matched < len)
			{
			    n = SSCANF(s, "-- %d:%d:%d:%*d%n", &hour, &min,
				       &sec, &matched);
			    if (matched < len)
			    {
				sec = 0;
				n = SSCANF(s, "-- %d:%d%n", &hour, &min,
					   &matched);
				if (matched < len)
				    return -1;
			    }
			}
		    }
		}
	    }
	}
    }

    time_tm.tm_sec = sec;
    time_tm.tm_min = min;
    time_tm.tm_hour = hour;
    if (*month)
	time_tm.tm_mon = getmon(month);
    else
	time_tm.tm_mon = now_tm->tm_mon;
    time_tm.tm_mday = day;
    time_tm.tm_year = year-1900;
    time_tm.tm_isdst = -1;

    *timep = mktime(&time_tm);

    if (*timep < 0)
	return -1;

    return 0;
}



static int getmon (char *month)
{
    char	*p;
    int		i;
    static char	*m[] = { "jan", "feb", "mar", "apr", "may", "jun",
		        "jul", "aug", "sep", "oct", "nov", "dec" };

    for(p=month; *p; p++)
	if (ISUPPER(*p))
	    *p = TOLOWER(*p);

    for(i=0; i<12; i++)
	if (!strcmp(month,m[i]))
	    return i;

    return -1;
}
