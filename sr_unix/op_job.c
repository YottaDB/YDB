/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_stdio.h"

#include <sys/wait.h>
#include <errno.h>

#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "iotimer.h"
#include "job.h"
#include "joberr.h"
#include "gt_timer.h"
#include "util.h"
#include "outofband.h"
#include "op.h"
#include "io.h"
#include "mvalconv.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#include "iosocketdef.h"
#include "min_max.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

void	job_timer_handler(void);

GBLDEF	short			jobcnt		= 0;
GBLDEF	volatile boolean_t	ojtimeout	= TRUE;

GBLREF	uint4		dollar_trestart;
GBLREF	int		dollar_truth;
GBLREF	uint4		dollar_zjob;
GBLREF	int4		outofband;
GBLREF	d_socket_struct	*socket_pool;
static	int4	tid;	/* Job Timer ID */

LITREF mval		skiparg;

error_def(ERR_JOBFAIL);
error_def(ERR_JOBLVN2LONG);
error_def(ERR_NULLENTRYREF);
error_def(ERR_TEXT);

#define JOBTIMESTR "JOB"

/*
 * ---------------------------------------------------
 * This handler is executed if job could not be started
 * during the specified timeout period
 * ---------------------------------------------------
 */
void	job_timer_handler(void)
{
	ojtimeout = TRUE;
}

/*
 * ---------------------------------------------------
 * Job command main entry point
 * ---------------------------------------------------
 */
int	op_job(int4 argcnt, ...)
{
	va_list			var;
	int4			i;
	mval			*label;
	int4			offset;
	mval			*routine, *param_buf;
	mval			*timeout;	/* timeout in milliseconds */
	int4			msec_timeout;	/* timeout in milliseconds */
	boolean_t		timed, single_attempt, non_exit_return;
	unsigned char		buff[128], *c;
	int4			status, exit_stat, term_sig, stop_sig;
	pid_t			zjob_pid = 0; 	/* zjob_pid should exactly match in type with child_pid(ojstartchild.c) */
	int			pipe_fds[2], pipe_status;
#	ifdef _BSD
	union wait		wait_stat;
#	else
	int4			wait_stat;
#	endif
	job_params_type		job_params;
	char			combuf[128];
	mstr			command;
	job_parm		*jp;
	mstr_len_t		handle_len;
	int4			index;
	job_buffer_size_msg	buffer_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, argcnt);
	assert(argcnt >= 5);
	label = va_arg(var, mval *);
	offset = va_arg(var, int4);
	routine = va_arg(var, mval *);
	param_buf = va_arg(var, mval *);
	timeout = va_arg(var, mval *);	/* in milliseconds */
	argcnt -= 5;
	/* initialize $zjob = 0, in case JOB fails */
	dollar_zjob = 0;
	MV_FORCE_DEFINED(label);
	MV_FORCE_DEFINED(routine);
	MV_FORCE_DEFINED(param_buf);
	/* create a pipe to channel the PID of the jobbed off process(J) from middle level
	 * process(M) to the current process (P)
	 */
	OPEN_PIPE(pipe_fds, pipe_status);
	if (-1 == pipe_status)
	{
		va_end(var);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_LIT("Error creating pipe"), errno);
	}
	jobcnt++;
	command.addr = &combuf[0];
	/* Setup job parameters by parsing param_buf and using label, offset, routine, & timeout).  */
	job_params.routine = routine->str;
	job_params.label = label->str;
	job_params.offset = offset;
	ojparams(param_buf->str.addr, &job_params);
	/*
	 * Verify that entryref to JOB command is not NULL.
	 */
	if (!job_params.routine.len)
	{
		va_end(var);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JOBFAIL, 0, ERR_NULLENTRYREF, 0);
	}
	/* Start the timer */
	ojtimeout = timed = FALSE;
	MV_FORCE_MSTIMEOUT(timeout, msec_timeout, JOBTIMESTR);
	if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
	{
		timed = TRUE;
		start_timer((TID)&tid, msec_timeout, job_timer_handler, 0, NULL);
	}
	if (argcnt)
	{
		jp = job_params.parms = (job_parm *)malloc(SIZEOF(job_parm) * argcnt);
		i = argcnt;
		for(;;)
		{
			jp->parm = va_arg(var, mval *);
			if (!M_ARG_SKIPPED(jp->parm))
				MV_FORCE_STR(jp->parm);
			if (0 == --i)
				break;
			jp->next = jp + 1;
			jp = jp->next;
		}
		jp->next = 0;
	} else
		job_params.parms = 0;
	va_end(var);
	/* Setup parameters and start the job */
	job_errno = -1;
	non_exit_return = FALSE;
	status = ojstartchild(&job_params, argcnt, &non_exit_return, pipe_fds);
	/* the child process (M), that wrote to pipe, would have been exited by now. Close the write-end to make the following read
	 * non-blocking. also resets "pipe_fds[1]" to FD_INVALID
	 */
	CLOSEFILE_RESET(pipe_fds[1], pipe_status);
	if (!non_exit_return)
	{
#ifdef _BSD
		assert(SIZEOF(wait_stat) == SIZEOF(int4));
		wait_stat.w_status = status;
		/* waitpid in ojstartchild() expects an int wait_status whereas the WIF* macros expect a union wait_stat as an
		 * argument.
		 */
#else
		wait_stat = status;
#endif
		exit_stat = WIFEXITED(wait_stat) ? WEXITSTATUS(wait_stat) : 0;
		/* Middle process uses pipe(pipe_fds)
		 *	a) to communicate a PID of grandchild to its parent process(i.e. current process)
		 *	b) to communicate an errno to current process if any required setup for the grandchild is failed.
		 * exit status joberr_pipe_mp of middle process means it failed WRITE operation on pipe used to communicate
		 * grandchild's PID to current process. In this scenario, grandchild is terminated and middle process do not
		 * communicate  errno to current process. Hence this process do not read the errno for joberr_pipe_mp exit status.
		 */
		if (status && joberr_pipe_mp != exit_stat)
		{
			DOREADRC(pipe_fds[0], &job_errno, SIZEOF(job_errno), pipe_status);
			if (0 < pipe_status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, joberrs[exit_stat].len,
										joberrs[exit_stat].msg, 2, errno);
			if (ERR_JOBLVN2LONG == job_errno)
			{	/* This message takes buffer_size as argument so take it before closing the pipe */
				DOREADRC(pipe_fds[0], &buffer_size, SIZEOF(buffer_size), pipe_status);
				if (0 < pipe_status)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						      LEN_AND_LIT("Error reading buffer_size from pipe after a JOBLVN2LONG error"),
						      errno);
			}
		}
	}
	if (argcnt)
		free(job_params.parms);
	if (timed && !ojtimeout)
		cancel_timer((TID)&tid);
	assert(SIZEOF(pid_t) == SIZEOF(zjob_pid));
	DOREADRC(pipe_fds[0], &zjob_pid, SIZEOF(zjob_pid), pipe_status);
	/* empty pipe (pipe_status == -1) is ignored and not reported as error */
	if (0 < pipe_status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error reading zjobid from pipe"), errno);
	/* release the pipe; also resets "pipe_fds[0]" to FD_INVALID */
	CLOSEFILE_RESET(pipe_fds[0], pipe_status);
	if (status)
	{
		if (timed)					/* $test should be modified only for timed job commands */
			dollar_truth = 0;
		if (non_exit_return)
		{
			if (TIMEOUT_ERROR != status) 		/* one of errno returns, not the wait_status/timeout situation */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_JOBFAIL, 0, status);
			else
				return FALSE;
		} else							/* wait_status from the child */
		{
			if (WIFSIGNALED(wait_stat))			/* child was SIGNALed and TERMINated */
			{
				term_sig =  WTERMSIG(wait_stat);	/* signal that caused the termination */
				memcpy(buff, joberrs[joberr_sig].msg, joberrs[joberr_sig].len);
				c = i2asc(&buff[joberrs[joberr_sig].len], term_sig);
				assert(FALSE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2, c - buff, buff);
			} else if (WIFSTOPPED(wait_stat))		/* child was STOPped */
			{
				stop_sig =  WSTOPSIG(wait_stat);	/* signal that caused the stop */
				memcpy(buff, joberrs[joberr_stp].msg, joberrs[joberr_stp].len);
				c = i2asc(&buff[joberrs[joberr_stp].len], stop_sig);
				assert(FALSE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2, c - buff, buff);
			} else if (WIFEXITED(wait_stat))			/* child EXITed normally */
			{
				if (exit_stat < joberr_stp)		/* one of our EXITs */
				{
					if (-1 == job_errno)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2,
							  joberrs[exit_stat].len,
							  joberrs[exit_stat].msg);
					} else if (ERR_JOBLVN2LONG == job_errno)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JOBLVN2LONG, 2, MAX_STRLEN,
							      buffer_size);
					} else
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
							  joberrs[exit_stat].len,
							  joberrs[exit_stat].msg,
							  job_errno);
					}
				} else					/* unknown exit status */
				{
					assert(FALSE);
					util_out_print("Unknown exit status !UL (status = !UL)", TRUE, exit_stat, status);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						  joberrs[joberr_gen].len, joberrs[joberr_gen].msg);
				}
			} else
			{
				assert(FALSE);
				util_out_print("Unknown wait status !UL", TRUE, status);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						joberrs[joberr_gen].len, joberrs[joberr_gen].msg);
			}
		}
	} else
	{
		if (timed)
			dollar_truth = 1;
		assert(0 < zjob_pid);
		dollar_zjob = zjob_pid;
		if (IS_JOB_SOCKET(job_params.input.addr, job_params.input.len))
		{
			handle_len = JOB_SOCKET_HANDLE_LEN(job_params.input.len);
			index = iosocket_handle(JOB_SOCKET_HANDLE(job_params.input.addr), &handle_len, FALSE, socket_pool);
			if (-1 != index)
				iosocket_close_one(socket_pool, index);
		}
		if (IS_JOB_SOCKET(job_params.output.addr, job_params.output.len))
		{
			handle_len = JOB_SOCKET_HANDLE_LEN(job_params.output.len);
			index = iosocket_handle(JOB_SOCKET_HANDLE(job_params.output.addr), &handle_len, FALSE, socket_pool);
			if (-1 != index)
				iosocket_close_one(socket_pool, index);
		}
		if (IS_JOB_SOCKET(job_params.error.addr, job_params.error.len))
		{
			handle_len = JOB_SOCKET_HANDLE_LEN(job_params.error.len);
			index = iosocket_handle(JOB_SOCKET_HANDLE(job_params.error.addr), &handle_len, FALSE, socket_pool);
			if (-1 != index)
				iosocket_close_one(socket_pool, index);
		}
		return TRUE;
	}
	return FALSE; /* This will never get executed, added to make compiler happy */
}
