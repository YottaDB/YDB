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

/* gtm_pipe.c

	Parameters --

		command :	the command to be executed.
		pt	:	type of the pipe (input or output)

	Returns	--

		-1	:	fork error
		-2	:	pipe() error
		other	:	the file descriptor serving as one end of the pipe
*/
#include "mdef.h"

#include <sys/types.h>

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"

#include "gtm_pipe.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "fork_init.h"

GBLDEF	uint4	pipe_child;

int gtm_pipe(char *command, pipe_type pt)
{
	int 	pfd[2], child, parent;
	int	dup2_res, rc;
	pid_t	child_pid;

	parent = (int)pt;
	child  = 1 - parent;
	if (0 > pipe(pfd))
	{
		PERROR("pipe : ");
		return -2;
	}
	FORK(child_pid);	/* BYPASSOK: we exit immediately, no FORK_CLEAN needed */
	if (-1 == child_pid)
	{
		PERROR("fork : ");
		return -1;
	} else if (0 == child_pid)
	{	/* child process */
		CLOSEFILE_RESET(pfd[parent], rc);	/* resets "pfd[parent]" to FD_INVALID */
		DUP2(pfd[child], child, dup2_res);
		CLOSEFILE_RESET(pfd[child], rc);	/* resets "pfd[child]" to FD_INVALID */
		/* We should have used exec instead of SYSTEM. Earlier it was followed by exit(EXIT_SUCCESS), which calls
		 * exit_handler.  So both child and parent will do exit handling. This can make ref_cnt < 0, or, it can release
		 * semaphores, which we should not release until parent exists. So we just call _exit(EXIT_SUCCESS).  Add the do
		 * nothing if to keep compiler happy since exiting anyway.
		 */
		rc = SYSTEM(command);
		_exit(EXIT_SUCCESS); /* just exit from here */
	} else
	{	/* parent process */
		pipe_child = child_pid;
		CLOSEFILE_RESET(pfd[child], rc);	/* resets "pfd[child]" to FD_INVALID */
   		return pfd[parent];
	}
}
