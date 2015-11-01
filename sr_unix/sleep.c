/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <time.h>
#include "sleep.h"

#ifndef __MVS__

int m_sleep(int seconds)
{
    m_time_t rqtp, rmtp;

    rqtp.tv_sec=seconds;
    rqtp.tv_nsec=0;

    return nanosleep_func(&rqtp,&rmtp);
}

int m_usleep(int useconds)
{
    m_time_t rqtp, rmtp;

    rqtp.tv_sec=0;
    rqtp.tv_nsec=useconds*1000;

    return nanosleep_func(&rqtp,&rmtp);
}

int m_nsleep(int nseconds)
{
    m_time_t rqtp, rmtp;

    rqtp.tv_sec=0;
    rqtp.tv_nsec=nseconds;

    return nanosleep_func(&rqtp,&rmtp);
}

#else

/* OS390 versions must use usleep, which doesn't use m_time_t struct */

#include "gtm_unistd.h"

int m_sleep(int seconds)
{
    return nanosleep_func((useconds_t)(1000 * seconds));
}

int m_usleep(int useconds)
{
    return nanosleep_func((useconds_t)useconds);
}

/* Note: this functions is not called by anyone */
int m_nsleep(int nseconds)	/* On OS390, this will sleep for one microsecond */
{
    return nanosleep_func((useconds_t)1);
}

#endif
