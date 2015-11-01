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

 /*
 * -------------------------------------------------------
 * This routine starts a new child process and passes the
 * job parameters to it.
 * -------------------------------------------------------
 */
#include "mdef.h"

#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include "gtm_fcntl.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_string.h"

#include "job.h"
#include "error.h"
#include "rtnhdr.h"
#include "io.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "compiler.h"
#include "job_addr.h"
#include "util.h"

#define MAX_CMD_LINE	8192	/* Maximum command line length */
#define MAX_PATH	 128	/* Maximum file path length */
#define MAX_LAB_LEN	  32	/* Maximum Label string length */
#define MAX_RTN_LEN	  32	/* Maximum Routine string length */
#define TEMP_BUFF_SIZE  1024
#define PARM_STRING_SIZE   9
#define MAX_NUM_LEN	  10	/* Maximum length number will be when converted to string */
#define MAX_JOB_QUALS	  12	/* Maximum environ variables set for job qualifiers */

static 	int		joberr = joberr_gen;
static 	boolean_t	job_launched = FALSE;

GBLREF	bool			jobpid;		/* job's output files should have the pid appended to them. */
GBLREF	volatile boolean_t    	ojtimeout;
GBLREF	boolean_t    		job_try_again;
GBLREF	uint4			process_id;
#if !defined(__MVS__) && !defined(__linux__)
GBLREF	int			sys_nerr;
#endif

#ifdef	__osf__
/* environ is from the O/S which only uses 64-bit pointers on OSF/1. */
#pragma	pointer_size (save)
#pragma pointer_size (long)
#endif

GBLREF char	**environ;

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

#define KILL_N_REAP(PROCESS_ID, SIGNAL, RET_VAL)					\
{											\
	if (-1 != (RET_VAL = kill(PROCESS_ID, SIGNAL)))					\
	{										\
		/* reap the just killed child, so there wont be any zombies */		\
		WAITPID(PROCESS_ID, &wait_status, 0, done_pid);				\
		assert(done_pid == PROCESS_ID);						\
	}										\
}

/* Note that this module uses _exit instead of exit to avoid running the inherited
   exit handlers which this mid-level process does not want to run */

/* if we get an error assembling the child environment,
	we don't want a rogue child running loose */

void job_term_handler(int sig);

static CONDITION_HANDLER(bad_child)
{
	PRN_ERROR;
	_exit(joberr);
}

#define FAILED_TO_LAUNCH 1

/* This is to close the window of racing condition where the timeout occurs and actually
 * by that time, the middle process had already successfully forked the job
 */

void job_term_handler(int sig){
	if (job_launched)
		_exit(0);
	else
		_exit(FAILED_TO_LAUNCH);
}

/*
 * ---------------------------------------------------------------------------------------------------------------------
 * The current process (P) FORKs a middle child process (M) that tests various job parameters. It then forks off the
 * actual Job (J) and exits, culminating the parent's (P) wait. The Job process (J) sets up its env and exexs mumps.
 *
 * Arguments
 * 	First argument is a pointer to the structure holding Job parameter values.
 * 	Second argument is the number of parameters being passed.
 * 	The third boolean argument indicates to the caller if the return from this function was due to an exit from the
 *		middle process or due to reasons other than that. It is set to true for the latter case of return.
 *
 * Return:
 *	Exit status of child (that the parent gets by WAITing) in case the return was after an exit from the middle process.
 *	errno in other cases with the third argument set to TRUE and returned by pointer.
 *	TIMEOUT_ERROR in case a timeout occured.
 *	Return zero indicates success.
 * ---------------------------------------------------------------------------------------------------------------------
 */

int ojstartchild (job_params_type *jparms, int argcnt, boolean_t *non_exit_return)
{
	char			cbuff[TEMP_BUFF_SIZE], pbuff[TEMP_BUFF_SIZE];
	char			tbuff[MAX_CMD_LINE], tbuff2[MAX_CMD_LINE], parm_string[PARM_STRING_SIZE];
	char			*pgbldir_str;
	char			*transfer_addr;
	int4			i, environ_count, string_len, temp;
	int			wait_status, save_errno, kill_ret;
	int			rc;
	bool			status;
	pid_t			par_pid, child_pid, done_pid;
	job_parm		*jp;
	rhdtyp			*base_addr;
	struct sigaction	act, old_act;

#ifdef	__osf__
/* These must be O/S-compatible 64-bit pointers for OSF/1.  */
#pragma	pointer_size (save)
#pragma pointer_size (long)
#endif

	char		*c1, *c2, **c3;
	char		*argv[3];
	char		**env_ary, **env_ind;

#ifdef	__osf__
#pragma pointer_size (restore)
#endif
	error_def(ERR_JOBPARTOOLONG);
	error_def(ERR_JOBFAIL);
	error_def(ERR_TEXT);

	job_launched = FALSE;
	par_pid = process_id;
	if (-1 == (child_pid = fork()))
	{
		if (EAGAIN == errno || ENOMEM == errno)
			job_try_again = TRUE;
		*non_exit_return = TRUE;
		return (errno);
	}

	if (child_pid == 0)
	{
		/* This is a child process (middle process, M) */
		/* Test out various parameters and setup everything possible for the actual Job (J), so it(J) can
		 * start off without much hitch. If any error occurs during this, exit with appropriate status so
		 * the waiting parent can diagnose.
		 */

		joberr = joberr_gen;
		ESTABLISH_RET(bad_child, 0);

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = job_term_handler;
		sigaction(SIGTERM, &act, &old_act);

		if (!jobpid)		/* if the Job pid need not be appended to the std-in/out/err file names */
		{
			joberr = joberr_io;
			/* attempt to open output files */
			/* this also redirects stdin/out/err, so any error messages by this process during
			 * the creation of the Job will get redirected */
			if (!(status = ojchildioset(jparms)))
			{
				if (job_try_again)
					joberr += joberr_tryagain;
				rts_error(VARLSTCNT(6) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Failed to set STDIN/OUT/ERR for the job"));
			}
		} /* else, all the errors during the creation of Job go to the STDOUT/ERR of the parent process */

		joberr = joberr_cd;		/* pass current directory to child */
		if (jparms->directory.len != 0)
		{
			/* If directory is specified, change it */
			if (jparms->directory.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);

			strncpy(pbuff, jparms->directory.addr, jparms->directory.len);
			*(pbuff + jparms->directory.len) = '\0';
			if (CHDIR(pbuff) != 0)
			{
				if (ETIMEDOUT == errno)		/* atleast on AIX */
					joberr += joberr_tryagain;
				rts_error(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error changing directory for the Job."),errno);
			}
		}

		joberr = joberr_rtn;
		job_addr(&jparms->routine, &jparms->label, jparms->offset, (char **)&base_addr, &transfer_addr);

		joberr = joberr_syscall;
                if (-1 == setsid())
			rts_error(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error setting session id for the Job."), errno);

		joberr = joberr_frk;
		if (0 != (child_pid = fork()))		/* clone self and exit */
		{
			/* This is still the middle process.  */
			if (0 > child_pid)
			{
				if (EAGAIN == errno || ENOMEM == errno)
					joberr += joberr_tryagain;
				rts_error(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error forking the Job."), errno);
			} else
			{
				job_launched = TRUE;
				_exit(0);
			}
		}

		/* This is now the grandchild process (actual Job process) -- an orphan as soon as the exit(0) above occurs. */
		/* set up the environment and exec */

		sigaction(SIGTERM, &old_act, 0);		/* restore the SIGTERM handler */

		joberr = joberr_io;
		io_rundown(RUNDOWN_EXCEPT_STD);
		DUP2(1, 2, rc); /* equate stdout, stderr */
		if (-1 == rc)
			rts_error(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error duping STDERR to STDOUT."), errno);

		/* Count the number of environment variables.  */
		for (environ_count = 0, c3 = environ, c2 = *c3;  c2;  c3++, c2 = *c3)
			environ_count++;

#ifdef	__osf__
/* Since we're creating an array of pointers for the O/S, make sure sizeof(char *) is correct for 64-bit pointers for OSF/1.  */
#pragma	pointer_size (save)
#pragma pointer_size (long)
#endif

		env_ind = env_ary = (char **)malloc((environ_count + MAX_JOB_QUALS + argcnt + 1)*sizeof(char *));

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

		string_len = strlen("%s=%d") + strlen(CHILD_FLAG_ENV) + MAX_NUM_LEN - 4;
		if (string_len > MAX_CMD_LINE)
			rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
		c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
		SPRINTF_ENV_NUM(CHILD_FLAG_ENV, par_pid);
#ifdef __MVS__
#pragma convlit(resume)
#endif
		*env_ind++ = c1;

		/*
		 * Pass all information about the job via shell's environment.
		 * The grandchild will get those variables to obtain the info about the job.
		 */

		/* pass global directory to child */
		if (jparms->gbldir.len != 0)
		{
			if (jparms->gbldir.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->gbldir.addr, jparms->gbldir.len);
			*(pbuff + jparms->gbldir.len) = '\0';
			string_len = strlen("%s=%s") + strlen(GBLDIR_ENV) + strlen(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(GBLDIR_ENV, pbuff);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}
		/* pass startup program to child */
		if (jparms->startup.len != 0)
		{
			if (jparms->startup.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->startup.addr, jparms->startup.len);
			*(pbuff + jparms->startup.len) = '\0';
			string_len = strlen("%s=%s") + strlen(STARTUP_ENV) + strlen(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(STARTUP_ENV, pbuff);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}
		/* pass input file to child */
		if (jparms->input.len != 0)
		{
			if (jparms->input.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->input.addr, jparms->input.len);
			*(pbuff + jparms->input.len) = '\0';
			string_len = strlen("%s=%s") + strlen(IN_FILE_ENV) + strlen(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(IN_FILE_ENV, pbuff);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}

		/* pass output file to child */
		if (jparms->output.addr != 0)
		{
			if (jparms->output.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->output.addr, jparms->output.len);
			*(pbuff + jparms->output.len) = '\0';
			if (jobpid)
				SPRINTF(&pbuff[jparms->output.len], ".%d", getpid());
			string_len = strlen("%s=%s") + strlen(OUT_FILE_ENV) + strlen(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(OUT_FILE_ENV, pbuff);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}

		/* pass error file to child */
		if (jparms->error.len != 0)
		{
			if (jparms->error.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->error.addr, jparms->error.len);
			*(pbuff + jparms->error.len) = '\0';
			if (jobpid)
				SPRINTF(&pbuff[jparms->error.len], ".%d", getpid());
			string_len = strlen("%s=%s") + strlen(ERR_FILE_ENV) + strlen(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(ERR_FILE_ENV, pbuff);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}

		/* pass routine name to child */
		if (jparms->routine.len != 0)
		{
			if (jparms->routine.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->routine.addr, jparms->routine.len);
			*(pbuff + jparms->routine.len) = '\0';
			string_len = strlen("%s=%s") + strlen(ROUTINE_ENV) + strlen(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(ROUTINE_ENV, pbuff);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}

		/* pass label name to child */
		if (jparms->label.len != 0)
		{
			if (jparms->label.len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->label.addr, jparms->label.len);
			*(pbuff + jparms->label.len) = '\0';
			string_len = strlen("%s=%s") + strlen(LABEL_ENV) + strlen(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(LABEL_ENV, pbuff);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}

		/* pass the offset */
		string_len = strlen("%s=%ld") + strlen(OFFSET_ENV) + MAX_NUM_LEN - 5;
		if (string_len > TEMP_BUFF_SIZE)
			rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
		c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
		SPRINTF_ENV_NUM(OFFSET_ENV, jparms->offset);
#ifdef __MVS__
#pragma convlit(resume)
#endif
		*env_ind++ = c1;

		/* pass Priority to child */
		if (jparms->baspri != 0)
		{
			string_len = strlen("%s=%ld") + strlen(PRIORITY_ENV) + MAX_NUM_LEN - 5;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_NUM(PRIORITY_ENV, jparms->baspri);
#ifdef __MVS__
#pragma convlit(resume)
#endif
			*env_ind++ = c1;
		}

		memcpy(parm_string, "gtmj000=", PARM_STRING_SIZE);
		for (i = 0, jp = jparms->parms;  jp ;  i++, jp = jp->next)
		{
			if (jp->parm->str.len > MAX_CMD_LINE - 2)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			string_len = strlen(parm_string) + jp->parm->str.len + 1;
			if (string_len > MAX_CMD_LINE)
				rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len);
#ifdef __MVS__
			__getEstring1_a_copy(c1, parm_string, strlen(parm_string));
			__getEstring1_a_copy(c1 + strlen(parm_string), jp->parm->str.addr, jp->parm->str.len);
#else
			memcpy(c1, parm_string, strlen(parm_string));
			memcpy(c1 + strlen(parm_string), jp->parm->str.addr, jp->parm->str.len);
#endif
			*(c1 + string_len - 1) = 0;
			*env_ind++ = c1;
			if (parm_string[6] == '9')
			{
				if (parm_string[5] == '9')
				{
					parm_string[4] = parm_string[4] + 1;
					parm_string[5] = '0';
				} else
				{
					parm_string[5] = parm_string[5] + 1;
				}
				parm_string[6] = '0';
			} else
				parm_string[6] = parm_string[6] + 1;
		}
		string_len = strlen("%s=%ld") + strlen(GTMJCNT_ENV) + MAX_NUM_LEN - 5;
		if (string_len > TEMP_BUFF_SIZE)
			rts_error(VARLSTCNT(1) ERR_JOBPARTOOLONG);
		c1 = (char *)malloc(string_len + 1);
#ifdef __MVS__
#pragma convlit(suspend)
#endif
		SPRINTF_ENV_NUM(GTMJCNT_ENV, i);
#ifdef __MVS__
#pragma convlit(resume)
#endif
		*env_ind++ = c1;

#ifdef	__osf__
/* Make sure sizeof(char *) is correct.  */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

		memcpy(env_ind, environ, (environ_count + 1)*sizeof(char *));

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

		c1 = GETENV("gtm_dist");
		memcpy(tbuff, c1, strlen(c1));
		c2 = &tbuff[strlen(c1)];
		memcpy(c2, "/mumps", 7);

#ifdef __MVS__	/* use real memcpy to preserve env in native code set */
#pragma convlit(suspend)
		memcpy(cbuff, "-direct", 8);
#pragma convlit(resume)
#else
		memcpy(cbuff, "-direct", 8);
#endif

#ifdef __MVS__
		__getEstring1_a_copy(tbuff2, tbuff, strlen(tbuff));
		argv[0] = tbuff2;
#else
		argv[0] = tbuff;
#endif
		argv[1] = cbuff;
		argv[2] = (char *)0;

		EXECVE(tbuff, argv, env_ary);
		/* if we got here, error starting the Job */
		rts_error(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_LIT("Exec error in Job"), errno);
		REVERT;
	} else
	{
		/* Parent, wait for the Middle process */
		/* Wait ends under the following conditions
		 *	The middle process successfully forks off the actual Job or exits with error before doing that.
		 *	Timeout for the Job command.
		 * For the first case, return the wait status as got from wait() syscall.
		 * For the second condition, set non_exit_return to TRUE and return the relevant errno.
		 */

		do
			done_pid = waitpid(child_pid, &wait_status, 0);
		while(!ojtimeout && 0 > done_pid && EINTR == errno);
				/* note : macro, WAITPID would not be suitable here because we want to catch our timeout */
				/* waitpid expects an integer wait_status even for _BSD cases, but WIF* macros expect
				 * a union wait argument (on AIX) */
		if (done_pid == child_pid)
			return (wait_status);
                else if (0 > done_pid && EINTR == errno && TRUE == ojtimeout)
                {
			/* Kill the middle process with SIGTERM and check the exit status from
			 * the handler to see if the Middle process had actually successfully forked the Job */
			KILL_N_REAP(child_pid, SIGTERM, kill_ret);
			if (-1 == kill_ret && ESRCH == errno)		/* if the middle process finished by now */
			{
				WAITPID(child_pid, &wait_status, 0, done_pid);
				if (done_pid == child_pid)
					return (wait_status);
			} else if (-1 != kill_ret && done_pid == child_pid && 0 == wait_status)
				return 0;		/* timer popped in the window of child fork and middle process exit */
			*non_exit_return = TRUE;
			return TIMEOUT_ERROR;	/* return special value so as to eliminate the window where the timer
						 * might pop after this routine returns to the callee and before the callee
						 * analyses the return status (ojtimeout may not be a reliable indicator) */
		} else if (0 > done_pid)
		{
			*non_exit_return = TRUE;
			save_errno = errno;
			KILL_N_REAP(child_pid, SIGKILL, kill_ret);
			return (save_errno);
		} else if (0 == done_pid)		/* this case should never arise */
		{
			assert(FALSE);
			*non_exit_return = TRUE;
			KILL_N_REAP(child_pid, SIGKILL, kill_ret);
			return (EINVAL);		/* return some error condition */
		}
	}
}
