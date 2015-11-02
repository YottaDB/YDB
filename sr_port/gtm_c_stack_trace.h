/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_C_STACK_TRACE_H
#define GTM_C_STACK_TRACE_H

#ifdef VMS
#define GET_C_STACK_MULTIPLE_PIDS(MESSAGE, CNL_PID_ARRAY, MAX_PID_SLOTS, STUCK_CNT)
#define GET_C_STACK_FROM_SCRIPT(MESSAGE, WAITINGPID, BLOCKINGPID, COUNT)
#define GET_C_STACK_FOR_KIP(KIP_PIDS_ARR_PTR, TRYNUM, MAX_TRY, STUCK_CNT, MAX_PID_SLOTS)
#elif defined(UNIX)
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
	const int4	uintsize = SIZEOF(uint4), messagelen = STRLEN(MESSAGE);					\
	char		command[GTM_MAX_DIR_LEN * 2 + 3 * SIZEOF(uint4) + 5]; /*5 = 4(space) + 1 (NULL)*/	\
	char		*errptr, *currpos;									\
	int4 		gtmprocstuckstrlen;									\
	int		rs, save_errno;										\
	char 		*gtm_waitstuck_script;									\
	error_def(ERR_STUCKACT);										\
	error_def(ERR_SYSCALL);											\
	error_def(ERR_TEXT);											\
	gtm_waitstuck_script = GETENV("gtm_procstuckexec");							\
	if (NULL != gtm_waitstuck_script)									\
	{													\
		gtmprocstuckstrlen = STRLEN(gtm_waitstuck_script);						\
		memcpy(command, gtm_waitstuck_script, gtmprocstuckstrlen);					\
		currpos = (char *)command + gtmprocstuckstrlen;							\
		*currpos++ = ' ';										\
		memcpy(currpos, MESSAGE, messagelen);								\
		currpos += messagelen;										\
		*currpos++ = ' ';										\
		sprintf(currpos, "%u %u %u", WAITINGPID, BLOCKINGPID, COUNT);					\
		currpos = (char *)command + STRLEN(command);							\
		rs = SYSTEM((char *)command);									\
		send_msg(VARLSTCNT(6) ERR_STUCKACT, 4, STRLEN("SUCCESS"), rs ? "FAILURE" : "SUCCESS",  		\
			STRLEN(command), (char *)command);							\
		if (0 != rs)											\
		{												\
			save_errno = errno;									\
			send_msg(VARLSTCNT(8) ERR_SYSCALL, 6, LEN_AND_LIT("system"), CALLFROM, save_errno);	\
			errptr = (char *)STRERROR(save_errno);							\
			send_msg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(errptr));				\
		}												\
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
	)													\
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
