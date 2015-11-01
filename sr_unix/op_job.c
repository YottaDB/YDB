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

#include "mdef.h"

/* LinuxIA32/gcc needs stdio before varargs due to stdarg */
/* Linux390/gcc needs varargs first */
#ifdef EARLY_VARARGS
#include <varargs.h>
#include "gtm_stdio.h"
#else
#include "gtm_stdio.h"
#include <varargs.h>
#endif

#include <sys/wait.h>
#include <errno.h>

#include "gtm_string.h"
#include "iotimer.h"
#include "job.h"
#include "joberr.h"
#include "gt_timer.h"
#include "util.h"
#include "outofband.h"
#include "op.h"
#include "io.h"

GBLDEF	short			jobcnt		= 0;
GBLDEF	volatile boolean_t	ojtimeout	= TRUE;

GBLREF	int4		outofband;
GBLREF	int		dollar_truth;
GBLREF	boolean_t	job_try_again;
#if !defined(__MVS__) && !defined(__linux__)
GBLREF	int		sys_nerr;
#endif

static	int4	tid;	/* Job Timer ID */
void	job_timer_handler(void);

#define MAX_CHAR_CAPACITY	0xFF

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
int	op_job(va_alist)
va_dcl
{
	va_list		var, save;
	int4		argcnt, i;
	mval		*label, *inp;
	int4		offset;
	mval		*routine, *param_buf;
	int4		timeout;		/* timeout in seconds */
	int4		msec_timeout;		/* timeout in milliseconds */
	boolean_t	timed, single_attempt, non_exit_return;
	unsigned char	buff[128], *c;
	int4		status, exit_stat, term_sig, stop_sig;
#ifdef _BSD
	union wait	wait_stat;
#else
	int4		wait_stat;
#endif

	job_params_type job_params;
	char		combuf[128];
	mstr		command;
	job_parm	*jp;

	error_def(ERR_TEXT);
	error_def(ERR_JOBFAIL);

	VAR_START(var);
	argcnt = va_arg(var, int4);
	assert(argcnt >= 5);
	label = va_arg(var, mval *);
	offset = va_arg(var, int4);
	routine = va_arg(var, mval *);
	param_buf = va_arg(var, mval *);
	timeout = va_arg(var, int4);	/* in seconds */
	argcnt -= 5;

	routine->str.len = mid_len((mident *)routine->str.addr);

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
		jp = job_params.parms = (job_parm *)malloc(sizeof(job_parm) * argcnt);
		i = argcnt;
		for(;;)
		{
			inp = va_arg(var, mval *);
			MV_FORCE_STR(inp);
			jp->parm = inp;
			if (0 == --i)
				break;
			jp->next = jp + 1;
			jp = jp->next;
		}
		jp->next = 0;
	} else
		job_params.parms = 0;

	assert(joberr_tryagain + 1 == joberr_end);	/* they must be adjacent and the last two */
	assert((joberr_tryagain * 2 - 1) < MAX_CHAR_CAPACITY);
	/* Setup parameters and start the job */
	do
	{
		job_try_again = FALSE;
		non_exit_return = FALSE;
		status = ojstartchild(&job_params, argcnt, &non_exit_return);
		if (status && !non_exit_return)
		{
			/* check if it was a try_again kind of failure */
#ifdef _BSD
			assert(sizeof(wait_stat) == sizeof(int4));
			wait_stat.w_status = status;
				/* waitpid() in ojstartchild() expects an int wait_status whereas the WIF* macros expect a
				 * union wait_stat as an arg */
#else
			wait_stat = status;
#endif
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
		return TRUE;
	}
}
