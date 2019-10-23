/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* CTIME format also used by asctime: Fri Oct 23 13:58:14 2015 */
#define CTIME_STRFMT	"%a %b %d %H:%M:%S %Y\n"

#define STRFTIME(dest, maxsize, format, timeptr, res)				\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);		\
	res = strftime(dest, maxsize, format, timeptr);				\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);	\
}

/* To use GET_CUR_TIME macro these definitions are required
 * now_t now; char time_str[CTIME_BEFORE_NL + 2];
 */
typedef time_t	now_t;

#define CTIME_BEFORE_NL 24
/* #GTM_THREAD_SAFE : The below macro (GET_CUR_TIME) is thread-safe */
#define GET_CUR_TIME(time_str)													\
{																\
	char	*time_ptr = &time_str[0];											\
	now_t	now;														\
	intrpt_state_t		prev_intrpt_state;										\
																\
	if ((time_t)-1 == (now = time(NULL)))											\
		MEMCPY_LIT(time_ptr, "****** time failed *****\n"); /* keep string len same as CTIME_BEFORE_NL */		\
	else															\
	{															\
		/* Do not use GTM_CTIME as it uses "ctime" which is not thread-safe. Use "ctime_r" instead which is thread-safe	\
		 * We still need to disable interrupts (from external signals) to avoid hangs (e.g. SIG-15 taking us to		\
		 * generic_signal_handler -> send_msg_csa -> syslog which in turn could deadlock due to an in-progress		\
		 * "ctime_r" call. Note that the DEFER_INTERRUPTS and ENABLE_INTERRUPTS macro are a no-op in case		\
		 * "multi_thread_in_use" is TRUE but external signals are anyways disabled by "gtm_multi_thread" in that case.	\
		 */														\
		DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);							\
		time_ptr = ctime_r(&now, time_ptr);										\
		ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);						\
		if (NULL == time_ptr)												\
		{														\
			time_ptr = &time_str[0];										\
			MEMCPY_LIT(time_ptr, "***** ctime failed *****\n"); /* keep string len same as CTIME_BEFORE_NL */	\
		}														\
		/* else time_str[] already contains the filled in time */							\
	}															\
}

#define GTM_MKTIME(VAR, TIME)							\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);		\
	VAR = mktime(TIME);							\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);	\
}

#define GTM_GMTIME(VAR, TIME)							\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);		\
	VAR = gmtime(TIME);							\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);	\
}

#define GTM_LOCALTIME(VAR, TIME)						\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);		\
	VAR = localtime(TIME);							\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);	\
}

/* CTIME collides with linux define in termios */
#define GTM_CTIME(VAR, TIME)										\
{													\
	GBLREF	boolean_t	multi_thread_in_use;							\
	intrpt_state_t		prev_intrpt_state;							\
													\
	/* "ctime" is not thread-safe. Make sure threads are not in use by callers of GTM_CTIME */	\
	GTM_PTHREAD_ONLY(assert(!multi_thread_in_use));							\
	DEFER_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);					\
	VAR = ctime(TIME);										\
	ENABLE_INTERRUPTS(INTRPT_IN_X_TIME_FUNCTION, prev_intrpt_state);				\
}

#endif
