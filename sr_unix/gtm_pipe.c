/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

		-1	:	fork() error
		-2	:	pipe() error
		other	:	the file descriptor serving as one end of the pipe
*/
#include "mdef.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gtm_stdio.h"
#include <unistd.h>
#include "gtm_stdlib.h"

#include "gtm_pipe.h"
#include "eintr_wrappers.h"

GBLDEF	uint4	pipe_child;

int gtm_pipe(char *command, pipe_type pt)
{
	int 	pfd[2], child, parent;
	int	dup2_res;
	pid_t	child_pid;

	parent = (int)pt;
	child  = 1 - parent;

	if (0 > pipe(pfd))
	{
		PERROR("pipe : ");
		return -2;
	}

	if(-1 == (child_pid = fork()))
	{
		PERROR("fork : ");
		return -1;
	}
	else if (0 == child_pid)
	{
		/* child process */
		close(pfd[parent]);
		DUP2(pfd[child], child, dup2_res);
		close(pfd[child]);
		/*
		 * we should have used exec instead of SYSTEM.
		 * Earlier it was followed by exit(0), which calls exit_handler.
		 * So both child and parent will do exit handling. This can make ref_cnt < 0, or,
		 * it can release semaphores, which we should not rlease until parent exists.
		 * So we just call _exit(0)
		 */
		SYSTEM(command);
		_exit(0); /* just exit from here */
	}
	else
	{
		/* parent process */
		pipe_child = child_pid;
		close(pfd[child]);
   		return pfd[parent];
	}

	assert(FALSE);
	/* It should never get here, just to keep compiler happy. */
	return -3;
}
