/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_C_STACK_TRACE_H
#define GTM_C_STACK_TRACE_H

#include "gtm_stdio.h"	/* For SPRINTF */
#define ONCE 1
#define TWICE 2

#ifdef VMS
#define GET_C_STACK_MULTIPLE_PIDS(MESSAGE, CNL_PID_ARRAY, MAX_PID_SLOTS, STUCK_CNT)
#define GET_C_STACK_FROM_SCRIPT(MESSAGE, WAITINGPID, BLOCKINGPID, COUNT)
#define GET_C_STACK_FOR_KIP(KIP_PIDS_ARR_PTR, TRYNUM, MAX_TRY, STUCK_CNT, MAX_PID_SLOTS)
#elif defined(UNIX)
#include <errno.h>
#include "send_msg.h"
#include "wbox_test_init.h"
#include "gt_timer.h"
#include "gtm_logicals.h"
#include "trans_log_name.h"
#ifndef GDSFHEAD_H_INCLUDED
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#endif

error_def(ERR_STUCKACT);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

#define GET_C_STACK_MULTIPLE_PIDS(MESSAGE, CNL_PID_ARRAY, MAX_PID_SLOTS, STUCK_CNT)		\
{												\
	uint4		index;									\
	uint4		pid;									\
	GBLREF uint4	process_id;								\
												\
	for (index = 0; MAX_PID_SLOTS > index; index++)						\
	{											\
		pid = CNL_PID_ARRAY[index];							\
		if (0 != pid)									\
			GET_C_STACK_FROM_SCRIPT(MESSAGE, process_id, pid, STUCK_CNT);		\
	}											\
}
#define GET_C_STACK_FROM_SCRIPT(MESSAGE, WAITINGPID, BLOCKINGPID, COUNT)					\
{														\
	int4		messagelen, arr_len; 									\
	char 	 	*command;										\
	char		*errptr, *currpos;									\
	int		rs, save_errno;										\
	char 		*gtm_waitstuck_script;									\
	mstr		envvar_logical, trans;									\
	char		buf[MAX_TRANS_NAME_LEN];								\
	int		status;											\
														\
	DCL_THREADGBL_ACCESS;                                                                                   \
														\
	SETUP_THREADGBL_ACCESS;                                                                     		\
	if (!(TREF(gtm_waitstuck_script)).len)									\
	{													\
		envvar_logical.addr = GTM_PROCSTUCKEXEC;							\
		envvar_logical.len = SIZEOF(GTM_PROCSTUCKEXEC)- 1;						\
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&envvar_logical, &trans, buf, SIZEOF(buf), 		\
										do_sendmsg_on_log2long)))	\
		{												\
			assert(SIZEOF(buf) > trans.len);							\
			if (0 != trans.len)									\
			{											\
				(TREF(gtm_waitstuck_script)).len = trans.len;					\
				(TREF(gtm_waitstuck_script)).addr = (char *)malloc(trans.len);			\
				memcpy((TREF(gtm_waitstuck_script)).addr, trans.addr, trans.len);		\
			}											\
		}												\
	}													\
	if (0 != (TREF(gtm_waitstuck_script)).len)								\
	{													\
		messagelen = STRLEN(MESSAGE);									\
		arr_len = GTM_MAX_DIR_LEN + messagelen + 3 * SIZEOF(uint4) + 5;					\
		command = (char *)malloc (arr_len);								\
		memcpy(command, (TREF(gtm_waitstuck_script)).addr, (TREF(gtm_waitstuck_script)).len);		\
		currpos = (char *)command + (TREF(gtm_waitstuck_script)).len;					\
		*currpos++ = ' ';										\
		memcpy(currpos, MESSAGE, messagelen);								\
		currpos += messagelen;										\
		*currpos++ = ' ';										\
		SPRINTF(currpos, "%u %u %u", WAITINGPID, BLOCKINGPID, COUNT);					\
		assert (STRLEN(command) < arr_len);								\
		rs = SYSTEM((char *)command);									\
		if (0 != rs)											\
		{												\
			save_errno = errno;									\
			send_msg(VARLSTCNT(6) ERR_STUCKACT, 4, LEN_AND_LIT("FAILURE"), LEN_AND_STR(command));	\
			send_msg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("system"), CALLFROM, save_errno);	\
		} else												\
			send_msg(VARLSTCNT(6) ERR_STUCKACT, 4, LEN_AND_LIT("SUCCESS"), LEN_AND_STR(command));	\
		free((void *)command);										\
	}													\
}
#define GET_C_STACK_FOR_KIP(KIP_PIDS_ARR_PTR, TRYNUM, MAX_TRY, STUCK_CNT, MAX_PID_SLOTS)			\
{														\
	boolean_t	invoke_c_stack = FALSE;									\
	const char	*kip_wait_string = NULL;								\
														\
	DEBUG_ONLY(												\
		/* If we had waited for half the max time, get a C stack trace on the processes currently 	\
		 * doing the kill 										\
		 */												\
		if ((MAX_TRY / 2) == TRYNUM)									\
		{												\
			invoke_c_stack = TRUE;									\
			kip_wait_string = "KILL_IN_PROG_HALFWAIT";						\
		}												\
	)	\
	/* If we had waited for max time, get a C stack trace on the processes currently doing the kill		\
	 * irrespective of whether it's pro or dbg 								\
	 */													\
	if (MAX_TRY <= TRYNUM)											\
	{													\
		invoke_c_stack = TRUE;										\
		kip_wait_string = "KILL_IN_PROG_WAIT";								\
	}													\
	if (invoke_c_stack)											\
		GET_C_STACK_MULTIPLE_PIDS(kip_wait_string, KIP_PIDS_ARR_PTR, MAX_PID_SLOTS, STUCK_CNT);	\
}

#else
#error UNSUPPORTED PLATFORM
#endif
#endif
