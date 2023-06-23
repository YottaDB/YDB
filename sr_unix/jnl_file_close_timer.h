/****************************************************************
 *								*
 * Copyright (c) 2016-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JNL_FILE_CLOSE_TIMER_H_INCLUDED
#define JNL_FILE_CLOSE_TIMER_H_INCLUDED

#define OLDERJNL_CHECK_INTERVAL			((uint8)60 * NANOSECS_IN_SEC)

#define START_JNL_FILE_CLOSE_TIMER_IF_NEEDED										\
MBSTART {														\
	GBLREF	boolean_t	heartbeat_started;									\
	GBLREF	boolean_t	oldjnlclose_started;									\
	GBLREF	boolean_t	is_src_server;										\
	GBLREF	boolean_t	exit_handler_active;									\
															\
	/* Now that caller has opened a journal file, start a timer to detect if we have				\
	 * stale journal file descriptors and if so close them. But if we have already started exit			\
	 * processing, do not start a new timer as this timer is not a safe timer and starting unsafe			\
	 * timers can have issues if done while we are in exit processing. Since we are anyway exiting,			\
	 * it is okay to not start this timer as stale file descriptors	are not a concern when we are exiting.		\
	 * Hence the "!exit_handler_active" check below.								\
	 */														\
	if (!exit_handler_active && !oldjnlclose_started && !is_src_server)						\
	{														\
		start_timer((TID)jnl_file_close_timer, OLDERJNL_CHECK_INTERVAL,	jnl_file_close_timer, 0, NULL);		\
		oldjnlclose_started = TRUE; /* should always be set AFTER start_timer */				\
	}														\
} MBEND

void jnl_file_close_timer(void);

#endif
