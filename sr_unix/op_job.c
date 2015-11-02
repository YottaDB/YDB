/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

GBLDEF	short			jobcnt		= 0;
GBLDEF	volatile boolean_t	ojtimeout	= TRUE;

GBLREF	uint4		dollar_trestart;
GBLREF	int		dollar_truth;
GBLREF	uint4		dollar_zjob;
GBLREF	boolean_t	job_try_again;
GBLREF	int4		outofband;

error_def(ERR_TEXT);
error_def(ERR_JOBFAIL);

static	int4	tid;	/* Job Timer ID */
void	job_timer_handler(void);

#define MAX_CHAR_CAPACITY	0xFF
#define JOBTIMESTR "JOB time too long"


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
	va_list		var;
	int4		i;
	mval		*label, *inp;
	int4		offset;
	mval		*routine, *param_buf;
	int4		timeout;		/* timeout in seconds */
	int4		msec_timeout;		/* timeout in milliseconds */
	boolean_t	timed, single_attempt, non_exit_return;
	unsigned char	buff[128], *c;
	int4		status, exit_stat, term_sig, stop_sig;
	pid_t		zjob_pid = 0; /* zjob_pid should exactly match in type with child_pid(ojstartchild.c) */
	int		pipe_fds[2], pipe_status;
#	ifdef _BSD
	union wait	wait_stat;
#	else
	int4		wait_stat;
#	endif
	job_params_type job_params;
	char		combuf[128];
	mstr		command;
	job_parm	*jp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, argcnt);
	assert(argcnt >= 5);
	label = va_arg(var, mval *);
	offset = va_arg(var, int4);
	routine = va_arg(var, mval *);
	param_buf = va_arg(var, mval *);
	timeout = va_arg(var, int4);	/* in seconds */
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
		rts_error(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_LIT("Error creating pipe"), errno);
	}
	jobcnt++;
	command.addr = &combuf[0];
	/* Setup job parameters by parsing param_buf and using label, offset, routine, & timeout).  */
	job_params.routine = routine->str;
	job_params.label = label->str;
	job_params.offset = offset;
	ojparams(param_buf->str.addr, &job_params);
	/* Clear the buffers */
	flush_pio();
	/* Start the timer */
	ojtimeout = FALSE;
	single_attempt = FALSE;
	if (timeout < 0)
		timeout = 0;
	else if (TREF(tpnotacidtime) < timeout)
		TPNOTACID_CHECK(JOBTIMESTR);
	if (NO_M_TIMEOUT == timeout)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
	} else
	{
		timed = TRUE;
		msec_timeout = timeout2msec(timeout);
		if (msec_timeout > 0)
			start_timer((TID)&tid, msec_timeout, job_timer_handler, 0, NULL);
		else
			single_attempt = TRUE;
	}
	if (argcnt)
	{
		jp = job_params.parms = (job_parm *)malloc(SIZEOF(job_parm) * argcnt);
		i = argcnt;
		for(;;)
		{
			inp = va_arg(var, mval *);
			jp->parm = inp;
			if (0 == --i)
				break;
			jp->next = jp + 1;
			jp = jp->next;
		}
		jp->next = 0;
	} else
		job_params.parms = 0;
	va_end(var);
	assert(joberr_tryagain + 1 == joberr_end);	/* they must be adjacent and the last two */
	assert((joberr_tryagain * 2 - 1) < MAX_CHAR_CAPACITY);
	/* Setup parameters and start the job */
	do
	{
		job_try_again = FALSE;
		non_exit_return = FALSE;
		status = ojstartchild(&job_params, argcnt, &non_exit_return, pipe_fds);
		if (status && !non_exit_return)
		{
			/* check if it was a try_again kind of failure */
#	ifdef _BSD
			assert(SIZEOF(wait_stat) == SIZEOF(int4));
			wait_stat.w_status = status;
				/* waitpid in ojstartchild() expects an int wait_status whereas the WIF* macros expect a
				 * union wait_stat as an arg
				 */
#	else
			wait_stat = status;
#	endif
			if (WIFEXITED(wait_stat) && (joberr_tryagain < (exit_stat = WEXITSTATUS(wait_stat))))
			{
				/* one of try-again situations */
				job_try_again = TRUE;
				exit_stat -= joberr_tryagain;
				assert(exit_stat < joberr_stp);
			}
		}
	} while (!single_attempt && status && !ojtimeout && job_try_again);
	if (argcnt)
		free(job_params.parms);
	if (timed && !ojtimeout)
		cancel_timer((TID)&tid);
	/* the child process (M), that wrote to pipe, would have been exited by now */
	CLOSEFILE_RESET(pipe_fds[1], pipe_status);	/* close the write-end to make the following read non-blocking;
							 * also resets "pipe_fds[1]" to FD_INVALID
							 */
	assert(SIZEOF(pid_t) == SIZEOF(zjob_pid));
	DOREADRC(pipe_fds[0], &zjob_pid, SIZEOF(zjob_pid), pipe_status); /* read jobbed off PID from pipe */
	if (0 < pipe_status) /* empty pipe (pipe_status == -1) is ignored and not reported as error */
		rts_error(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_LIT("Error reading from pipe"), errno);
	CLOSEFILE_RESET(pipe_fds[0], pipe_status); /* release the pipe; also resets "pipe_fds[0]" to FD_INVALID */
	if (status)
	{
		if (timed)					/* $test should be modified only for timed job commands */
			dollar_truth = 0;
		if (non_exit_return)
		{
			if (TIMEOUT_ERROR != status) 		/* one of errno returns, not the wait_status/timeout situation */
				rts_error(VARLSTCNT(3) ERR_JOBFAIL, 0, status);
			else
				return FALSE;
		} else							/* wait_status from the child */
		{
			if (WIFSIGNALED(wait_stat))			/* child was SIGNALed and TERMINated */
			{
				term_sig =  WTERMSIG(wait_stat);	/* signal that caused the termination */
				memcpy(buff, joberrs[joberr_sig].msg, joberrs[joberr_sig].len);
				c = i2asc(&buff[joberrs[joberr_sig].len], term_sig);
				rts_error(VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2, c - buff, buff);
			} else if (WIFSTOPPED(wait_stat))		/* child was STOPped */
			{
				stop_sig =  WSTOPSIG(wait_stat);	/* signal that caused the stop */
				memcpy(buff, joberrs[joberr_stp].msg, joberrs[joberr_stp].len);
				c = i2asc(&buff[joberrs[joberr_stp].len], stop_sig);
				rts_error(VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2, c - buff, buff);
			} else if (WIFEXITED(wait_stat))			/* child EXITed normally */
			{
				if (exit_stat < joberr_stp)		/* one of our EXITs */
				{
					rts_error(VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						  joberrs[exit_stat].len, joberrs[exit_stat].msg);
				} else					/* unknown exit status */
				{
					assert(FALSE);
					util_out_print("Unknown exit status !UL (status = !UL)", TRUE, exit_stat, status);
					rts_error(VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						  joberrs[joberr_gen].len, joberrs[joberr_gen].msg);
				}
			} else
			{
				assert(FALSE);
				util_out_print("Unknown wait status !UL", TRUE, status);
				rts_error(VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						joberrs[joberr_gen].len, joberrs[joberr_gen].msg);
			}
		}
	} else
	{
		if (timed)
			dollar_truth = 1;
		assert(0 < zjob_pid);
		dollar_zjob = zjob_pid;
		return TRUE;
	}
	return FALSE; /* This will never get executed, added to make compiler happy */
}
