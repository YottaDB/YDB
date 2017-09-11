/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _JNL_FILE_CLOSE_TIMER_H
#define _JNL_FILE_CLOSE_TIMER_H

#define OLDERJNL_CHECK_INTERVAL			(60 * MILLISECS_IN_SEC)

#define START_JNL_FILE_CLOSE_TIMER_IF_NEEDED										\
MBSTART {														\
	GBLREF boolean_t heartbeat_started;										\
	GBLREF boolean_t oldjnlclose_started;										\
	GBLREF boolean_t is_src_server;											\
															\
	if (!oldjnlclose_started && !is_src_server)									\
	{														\
		start_timer((TID)jnl_file_close_timer, OLDERJNL_CHECK_INTERVAL,	jnl_file_close_timer, 0, NULL);		\
		oldjnlclose_started = TRUE; /* should always be set AFTER start_timer */				\
	}														\
} MBEND

void jnl_file_close_timer(void);

#endif
