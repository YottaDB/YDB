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

/* gtm_time.h - interlude to <time.h> system header file.  */
#ifndef GTM_TIMEH
#define GTM_TIMEH

#include <time.h>

#define STRFTIME(dest, maxsize, format, timeptr, res)	\
{							\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);	\
	res = strftime(dest, maxsize, format, timeptr);	\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);	\
}

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
	else							\
	{							\
		GTM_CTIME(time_ptr, &now);				\
		if (NULL == time_ptr)				\
			time_ptr = "***** ctime failed *****\n"; /* keep string len same as CTIME_BEFORE_NL */ \
		else						\
		{						\
			memcpy(time_str, time_ptr, CTIME_BEFORE_NL + 2);	\
			time_ptr = time_str;			\
		}						\
	}							\
}

#define GTM_MKTIME(VAR, TIME)					\
{								\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
	VAR = mktime(TIME);					\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
}

#define GTM_GMTIME(VAR, TIME)					\
{								\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
	VAR = gmtime(TIME);					\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
}

#endif /* UNIX, VMS */

#define GTM_LOCALTIME(VAR, TIME)				\
{								\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
	VAR = localtime(TIME);					\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
}

/* CTIME collides with linux define in termios */
#define GTM_CTIME(VAR, TIME)					\
{								\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
	VAR = ctime(TIME);					\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION);		\
}

#endif
