/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GETJOBNUM_H__
#define __GETJOBNUM_H__

#ifdef YDB_USE_POSIX_TIMERS
#	define	CLEAR_POSIX_TIMER_FIELDS_IF_APPLICABLE							\
	MBSTART {											\
		/* Since this macro is called usually on a fresh process startup or after a "fork",	\
		 * any thread id we stored (for "timer_create" call) is no longer valid so clear it	\
		 * in the "fork" case. Or else "sys_settimer" (in gt_timers.c) will incorrectly call	\
		 * "timer_create" with a thread id that is not in our process anymore (EINVAL).		\
		 */											\
		posix_timer_thread_id = 0;								\
		/* By the same reasoning, also clear any record that a posix timer was created */	\
		posix_timer_created = FALSE;								\
	} MBEND
#else
#	define	CLEAR_POSIX_TIMER_FIELDS_IF_APPLICABLE
#endif

#define	SET_PROCESS_ID										\
MBSTART {											\
	GBLREF	pid_t		posix_timer_thread_id;						\
	GBLREF	boolean_t	posix_timer_created;						\
	GBLREF	uint4		process_id;							\
												\
	process_id = (uint4)getpid();								\
	CLEAR_POSIX_TIMER_FIELDS_IF_APPLICABLE;							\
} MBEND

void getjobnum(void);

#endif
