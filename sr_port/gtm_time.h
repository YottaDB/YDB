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

/* gtm_time.h - interlude to <time.h> system header file.  */
#ifndef GTM_TIMEH
#define GTM_TIMEH

#include <time.h>

/* CTIME collides with linux define in termios */

#define GTM_CTIME	ctime

#define STRFTIME(dest, maxsize, format, timeptr, res)	\
		res = strftime(dest, maxsize, format, timeptr)

#define CTIME_BEFORE_NL 24

#endif
