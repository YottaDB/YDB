/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_C_STACK_TRACE_H
#define GTM_C_STACK_TRACE_H

#define ONCE 1
#define TWICE 2

#ifdef VMS
#define GET_C_STACK_MULTIPLE_PIDS(MESSAGE, CNL_PID_ARRAY, MAX_PID_SLOTS, STUCK_CNT)
#define GET_C_STACK_FROM_SCRIPT(MESSAGE, WAITINGPID, BLOCKINGPID, COUNT)
#define GET_C_STACK_FOR_KIP(KIP_PIDS_ARR_PTR, TRYNUM, MAX_TRY, STUCK_CNT, MAX_PID_SLOTS)
#elif defined(UNIX)

void gtm_c_stack_trace(char *message, pid_t waiting_pid, pid_t blocking_pid, uint4 count);

#define GET_C_STACK_MULTIPLE_PIDS(MESSAGE, CNL_PID_ARRAY, MAX_PID_SLOTS, STUCK_CNT)				\
{														\
	uint4		index;											\
	pid_t		pid;											\
	GBLREF uint4	process_id;										\
														\
	for (index = 0; MAX_PID_SLOTS > index; index++)								\
	{													\
		pid = CNL_PID_ARRAY[index];									\
		if (0 != pid)											\
			GET_C_STACK_FROM_SCRIPT(MESSAGE, process_id, pid, STUCK_CNT);				\
	}													\
}

#define GET_C_STACK_FROM_SCRIPT(MESSAGE, WAITINGPID, BLOCKINGPID, COUNT)					\
{														\
	gtm_c_stack_trace(MESSAGE, WAITINGPID, BLOCKINGPID, COUNT);						\
}

#define GET_C_STACK_FOR_KIP(KIP_PIDS_ARR_PTR, TRYNUM, MAX_TRY, STUCK_CNT, MAX_PID_SLOTS)			\
{														\
	boolean_t	invoke_c_stack = FALSE;									\
	char		*kip_wait_string = NULL;								\
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
		GET_C_STACK_MULTIPLE_PIDS(kip_wait_string, KIP_PIDS_ARR_PTR, MAX_PID_SLOTS, STUCK_CNT);		\
}

#else
#error UNSUPPORTED PLATFORM
#endif
#endif
