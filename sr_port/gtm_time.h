/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#define GTM_CTIME_R	ctime_r

#define STRFTIME(dest, maxsize, format, timeptr, res)	\
		res = strftime(dest, maxsize, format, timeptr)

/* To use GET_CUR_TIME macro these definitions are required
 * now_t now; char *time_ptr; char time_str[CTIME_BEFORE_NL + 2];
 */
#if defined(VMS)

typedef	struct {
	unsigned int	buff1;
	unsigned int	buff2;
} now_t;

#define CTIME_BEFORE_NL	20

#define GET_CUR_TIME 						\
{								\
	uint4	time_status;					\
	$DESCRIPTOR(atimenow, time_str); 			\
								\
	time_status = sys$asctim(0, &atimenow, 0, 0);		\
	if (0 != (time_status & 1))				\
	{							\
		time_str[CTIME_BEFORE_NL] = '\n';		\
		time_str[CTIME_BEFORE_NL + 1] = '\0';		\
		time_ptr = time_str;				\
	} else							\
		time_ptr = "* sys$asctim failed*\n"; 	/* keep string len same as CTIME_BEFORE_NL */ \
}

#elif defined(UNIX)

typedef time_t	now_t;

#define CTIME_BEFORE_NL 24
#define GET_CUR_TIME 						\
{		 						\
	if ((time_t)-1 == (now = time(NULL)))			\
		time_ptr = "****** time failed *****\n"; /* keep string len same as CTIME_BEFORE_NL */ \
	else if (NULL == (time_ptr = (char *)GTM_CTIME(&now)))	\
		time_ptr = "***** ctime failed *****\n"; /* keep string len same as CTIME_BEFORE_NL */ \
	else							\
	{							\
		memcpy(time_str, time_ptr, CTIME_BEFORE_NL + 2);	\
		time_ptr = time_str;				\
	}							\
}

#endif /* UNIX, VMS */

#endif
