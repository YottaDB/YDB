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
#include "gtm_limits.h"

#if defined(SYS_ERRLIST_INCLUDE) && !defined(__CYGWIN__)
#include SYS_ERRLIST_INCLUDE
#endif

#include "job.h"
#include "error.h"
#include <rtnhdr.h>
#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "compiler.h"
#include "job_addr.h"
#include "util.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gtmio.h"
#include "fork_init.h"

GBLREF	bool			jobpid;	/* job's output files should have the pid appended to them. */
GBLREF	volatile boolean_t	ojtimeout;
GBLREF  boolean_t       	gtm_pipe_child;
GBLREF	int			job_errno;

static  joberr_t		joberr = joberr_gen;
static	int			pipe_fd;

#ifndef SYS_ERRLIST_INCLUDE
/* currently either stdio.h or errno.h both of which are included above needed by TIMEOUT_ERROR in jobsp.h */
#if !defined(__sun) && !defined(___MVS__)
GBLREF	int			sys_nerr;
#endif
#endif

#ifdef	__osf__
/* environ is from the O/S which only uses 64-bit pointers on OSF/1. */
#pragma	pointer_size (save)
#pragma pointer_size (long)
#endif

GBLREF char	**environ;
GBLREF char	gtm_dist[GTM_PATH_MAX];

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

#define MAX_JOB_LEN		8192	/* Arbitrary length maximum used for checking job arguments and parameters */
#define MAX_PATH		 128	/* Maximum file path length */
#define MAX_LAB_LEN		  32	/* Maximum Label string length */
#define MAX_RTN_LEN		  32	/* Maximum Routine string length */
#define TEMP_BUFF_SIZE		1024
#define PARM_STRING_SIZE	   9
#define MAX_NUM_LEN		  10	/* Maximum length number will be when converted to string */
#define MAX_JOB_QUALS		  12	/* Maximum environ variables set for job qualifiers */
#define	MUMPS_EXE_STR		"/mumps"
#define	MUMPS_DIRECT_STR	"-direct"
#define GTMJ_FMT		"gtmj%03d="
#define PARM_STR		"gtmj000="
#define	JOB_CONTINUE		1
#define JOB_EXIT		0

#define KILL_N_REAP(PROCESS_ID, SIGNAL, RET_VAL)					\
{											\
	if (-1 != (RET_VAL = kill(PROCESS_ID, SIGNAL)))					\
	{										\
		/* reap the just killed child, so there wont be any zombies */		\
		WAITPID(PROCESS_ID, &wait_status, 0, done_pid);				\
		assert(done_pid == PROCESS_ID);						\
	}										\
}

#define FORK_RETRY(PID)											\
{													\
	FORK(PID); /* BYPASSOK: we die after creating a child, no FORK_CLEAN needed */			\
	while (-1 == PID)										\
	{												\
		assertpro(EAGAIN == errno || ENOMEM == errno);						\
		usleep(50000);										\
		FORK(PID);	/* BYPASSOK: we die after creating a child, no FORK_CLEAN needed */	\
	}												\
}

error_def(ERR_JOBFAIL);
error_def(ERR_JOBPARTOOLONG);
error_def(ERR_LOGTOOLONG);
error_def(ERR_TEXT);

/* Note that this module uses _exit instead of exit to avoid running the inherited exit handlers which this mid-level process does
 * not want to run.
 */

void job_term_handler(int sig);
static int io_rename(job_params_type *jparms, const int jobid);

/* Middle process sets the entryref for the jobbed-off process by calling job_addr(). job_addr() internally calls op_zlink(). If
 * rts_error occurs when we are in the job_addr() function, the topmost condition handler inherited from parent process will be
 * executed. That condition handler might not be adequate to handle the rts_error happened in the context of middle process.
 * Hence middle process is provided with its own condition handler.
 */
static CONDITION_HANDLER(middle_child)
{
	int pipe_status;
	/* As of now, middle process could encounter rts_error only while setting entryref. Hence the following assert. */
	assert(joberr == joberr_rtn);
	DOWRITERC(pipe_fd, &job_errno, SIZEOF(job_errno), pipe_status);
	_exit(joberr);
}

/* Following condition handles the rts_error occurred in the grandchild before doing execv(). Sinec we have not started executing
 * M-cmd specified as a part of JOB command, it is enough to print the error and exit.
 */
static CONDITION_HANDLER(grand_child)
{
	PRN_ERROR;
	_exit(EXIT_SUCCESS);
}

/* This is to close the window of race condition where the timeout occurs and actually by that time, the middle process had already
 * successfully forked-off the job.
 */

void job_term_handler(int sig)
{
		int ret;
		int status;
		joberr_t exit_status = joberr_gen;
		/*
		 * ret	= 0 - Child is present but not changed the state
		 *	< 0 - Error. No child present.
		 *	> 0 - Child PID.
		 */
		ret = waitpid(-1, &status, WNOHANG);	/* BYPASSOK */
		job_errno = errno;
		if (0 == ret)
			return;
		else if ( 0 > ret)
			_exit(exit_status);
		else
			return;
}

static int io_rename(job_params_type *jparms, const int jobid)
{
	char path[MAX_STDIOE_LEN];

	strncpy(path, jparms->output.addr, jparms->output.len);
	*(jparms->output.addr + jparms->output.len) = '\0';
	SPRINTF(&path[jparms->output.len], ".%d", jobid);
	if (rename(jparms->output.addr, path))
	{
		job_errno = errno;
		return(joberr_stdout_rename);
	}
	strncpy(path, jparms->error.addr, jparms->error.len);
	*(jparms->error.addr + jparms->error.len) = '\0';
	SPRINTF(&path[jparms->error.len], ".%d", jobid);
	if (rename(jparms->error.addr, path))
	{
		job_errno = errno;
		return(joberr_stderr_rename);
	}
	return 0;
}

/*
 * --------------------------------------------------------------------------------------------------------------------------------
 * The current process (P) FORKs a middle child process (M) that tests various job parameters. It then forks off the actual Job (J)
 * and exits, culminating the parent's (P) wait. The Job process (J) sets up its env and execs mumps.
 *
 * Arguments
 * 	First argument is a pointer to the structure holding Job parameter values.
 * 	Second argument is the number of parameters being passed.
 * 	The third boolean argument indicates to the caller if the return from this function was due to an exit from the middle
 * 	process or due to reasons other than that. It is set to true for the latter case of return.
 *	Fourth argument is the pair of file descriptors	[opened by pipe] for the child process (M) to write PID of the jobbed off
 *	process (J).
 *
 * Return:
 *	Exit status of child (that the parent gets by WAITing) in case the return was after an exit from the middle process.
 * 	Errno in other cases with the third argument set to TRUE and returned by pointer.
 *	TIMEOUT_ERROR in case a timeout occured.
 *	Return zero indicates success.
 * --------------------------------------------------------------------------------------------------------------------------------
 */

int ojstartchild (job_params_type *jparms, int argcnt, boolean_t *non_exit_return, int pipe_fds[])
{
	char			cbuff[TEMP_BUFF_SIZE], pbuff[TEMP_BUFF_SIZE], cmdbuff[TEMP_BUFF_SIZE];
	char			tbuff[MAX_JOB_LEN], tbuff2[MAX_JOB_LEN], fname_buf[MAX_STDIOE_LEN];
	char			*pgbldir_str;
	char			*transfer_addr;
	int4			index, environ_count, string_len, temp;
	int			wait_status, save_errno, kill_ret;
	int			rc, dup_ret, in_fd;
	int			status;
	pid_t			par_pid, child_pid, done_pid;
	job_parm		*jp;
	rhdtyp			*base_addr;
	struct sigaction	act, old_act;
	int			pipe_status, env_len;
	int			mproc_fds[2]; 	/* pipe between middle process and grandchild process */
	int			decision;

#ifdef	__osf__
/* These must be O/S-compatible 64-bit pointers for OSF/1.  */
#pragma	pointer_size (save)
#pragma pointer_size (long)
#endif

	char		*c1, *c2, **c3;
	char		*argv[4];
	char		**env_ary, **env_ind;
	char		**new_env_cur, **new_env_top, **old_env_cur, **old_env_top, *env_end;

#ifdef	__osf__
#pragma pointer_size (restore)
#endif
	par_pid = getppid();

	FORK_RETRY(child_pid);
	if (child_pid == 0)
	{
		/* This is a child process (middle process, M)
		 * Test out various parameters and setup everything possible for the actual Job (J), so it(J) can start off without
		 * much hitch. If any error occurs during this, exit with appropriate status so the waiting parent can diagnose.
		 */

		/* set to TRUE so any child process associated with a pipe device will know it is not the parent in iorm_close() */
		gtm_pipe_child = TRUE;
		joberr = joberr_gen;
		job_errno = -1;
		pipe_fd = pipe_fds[1];
		ESTABLISH_RET(middle_child, 0);

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = job_term_handler;
		sigaction(SIGTERM, &act, &old_act);

		OPEN_PIPE(mproc_fds, pipe_status);
		if (-1 == pipe_status)
		{
			joberr = joberr_pipe_mgc;
			job_errno = errno;
			DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
			_exit(joberr);
		}
		/* Redirect input before potentially changing the default directory below.*/
		strncpy(fname_buf, jparms->input.addr, jparms->input.len);
		*(fname_buf + jparms->input.len) = '\0';

		OPENFILE(fname_buf, O_RDONLY, in_fd);
		if (FD_INVALID == in_fd)
		{
			joberr = joberr_io_stdin_open;
			job_errno = errno;
			return joberr;
		}
		CLOSEFILE(0, rc);
		FCNTL3(in_fd, F_DUPFD, 0, dup_ret);
		if (-1 == dup_ret)
		{
			joberr = joberr_io_stdin_dup;
			job_errno = errno;
			return joberr;
		}
#ifdef __MVS__
		/* policy tagging because by default input is /dev/null */
		if (-1 == gtm_zos_tag_to_policy(in_fd, TAG_UNTAGGED, &realfiletag))
			TAG_POLICY_SEND_MSG(fname_buf, errno, realfiletag, TAG_UNTAGGED);
#endif
		CLOSEFILE_RESET(in_fd, rc);	/* resets "in_fd" to FD_INVALID */
		if (0 != jparms->directory.len)
		{
			/* If directory is specified, change it */
			if (jparms->directory.len > TEMP_BUFF_SIZE)
			{
				joberr = joberr_cd_toolong;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
				_exit(joberr);
			}
			strncpy(pbuff, jparms->directory.addr, jparms->directory.len);
			*(pbuff + jparms->directory.len) = '\0';

			if (CHDIR(pbuff) != 0)
			{
				joberr = joberr_cd;
				job_errno = errno;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(errno), pipe_status);
				_exit(joberr);
			}
		}

		/* attempt to open output files. This also redirects stdin/out/err, so any error messages by this process during the
		 * creation of the Job will get redirected.
		 */
		if ((status = ojchildioset(jparms)))
		{
			DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
			_exit(status);
		}

		joberr = joberr_rtn;
		if (!job_addr(&jparms->routine, &jparms->label, jparms->offset, (char **)&base_addr, &transfer_addr))
		{
			DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
			_exit(joberr);
		}

		joberr = joberr_sid;
		if (-1 == setsid())
		{
			job_errno = errno;
			DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
			_exit(joberr);
		}

		/* clone self and exit */
		FORK_RETRY(child_pid);
		if (child_pid)	/* BYPASSOK: we exec immediately, no FORK_CLEAN needed */
		{
			/* This is still the middle process.  */
			/* Close the read end of the pipe between middle process and grandchild process. */
			CLOSEFILE_RESET(mproc_fds[0], pipe_status);	/* resets "pipe_fds[0]" to FD_INVALID */
			assert(SIZEOF(pid_t) == SIZEOF(child_pid));
			/* if the Job pid need to be appended to the std out/err file names */
			if (jobpid)
			{
				joberr = io_rename(jparms, child_pid);
				if (joberr)
				{
					/* Inform grandchild that it will have to exit. If pipe operation failed terminate
					 * the grandchild.
					 */
					decision = JOB_EXIT;
					DOWRITERC(mproc_fds[1], &decision, SIZEOF(decision), pipe_status);
					if (pipe_status)
						kill(child_pid, SIGTERM);
					DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
					_exit(joberr);
				}
			}
			/* write child_pid into pipe to be read by parent process(P) for $ZJOB */
			DOWRITERC(pipe_fds[1], &child_pid, SIZEOF(child_pid), pipe_status);
			/* Failed to send parent new JOBID. Terminate the child and exit middle process. */
			if (pipe_status)
			{
				/* Inform grandchild that it will have to exit. If pipe operation failed terminate the
				 * grandchild.
				 */
				joberr = joberr_pipe_mp;
				decision = JOB_EXIT;
				DOWRITERC(mproc_fds[1], &decision, SIZEOF(decision), pipe_status);
				if (pipe_status)
					kill(child_pid, SIGTERM);
				_exit(joberr);
			} else
			{
				/* Inform grandchild that everything is properly set for it and it is ready to continue. */
				decision = JOB_CONTINUE;
				DOWRITERC(mproc_fds[1], &decision, SIZEOF(decision), pipe_status);
				/* If pipe_status is non-zero i.e. error occurred in pipe operation or total
				 * SIZEOF(decision) bytes are not written in the pipe. In this case, we let the grandchild
				 * handle its own fate as we have already conveyed back the grandchild's pid to parent
				 * process and all the setup for the grandchild is successfully done.
				 */
			}
			_exit(EXIT_SUCCESS);
		}
		/* This is now the grandchild process (actual Job process) -- an orphan as soon as the exit(EXIT_SUCCESS) above
		 * occurs. Revert the condition handler established by middle process and establish its own condition handler */
		REVERT;
		ESTABLISH_RET(grand_child, 0);
		sigaction(SIGTERM, &old_act, 0);		/* restore the SIGTERM handler */
		CLOSEFILE_RESET(mproc_fds[1], pipe_status);	/* resets "mproc_fds[0]" to FD_INVALID */
		DOREADRC(mproc_fds[0], &decision, SIZEOF(decision), pipe_status);
		if (pipe_status)	 /* We failed to read the communication from middle process */
	                rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error reading from pipe"), errno);
		else {
			if (JOB_EXIT == decision)
				exit(EXIT_SUCCESS);
			assert(JOB_CONTINUE == decision);
		}
		CLOSEFILE_RESET(mproc_fds[0], pipe_status);	/* resets "mproc_fds[0]" to FD_INVALID */

		/* Run down any open flat files to reclaim their file descriptors */
		io_rundown(RUNDOWN_EXCEPT_STD);

		/* release the pipe opened by grand parent (P) */
		CLOSEFILE_RESET(pipe_fds[0], pipe_status);	/* resets "pipe_fds[0]" to FD_INVALID */
		CLOSEFILE_RESET(pipe_fds[1], pipe_status);	/* resets "pipe_fds[1]" to FD_INVALID */

		/* do common cleanup in child */
		ojchildioclean();

		/* Count the number of environment variables.  */
		for (environ_count = 0, c3 = environ, c2 = *c3;  c2;  c3++, c2 = *c3)
			environ_count++;
#ifdef	__osf__
/* Since we're creating an array of pointers for the O/S, make sure SIZEOF(char *) is correct for 64-bit pointers for OSF/1.  */
#pragma	pointer_size (save)
#pragma pointer_size (long)
#endif

		/* the environment array passed to the grandchild is constructed by prefixing the job related environment
		 * variables ahead of the current environment (pointed to by the "environ" variable)
		 *
		 * e.g. if the current environment has only two environment variables env1=one and env2=two,
		 * and the job command is as follows
		 * 	job ^x(1,2):(output="x.mjo":error="x.mje")
		 *
		 * then the environment array passed is as follows
		 *	 gtmj0=			// parent pid
		 *	 gtmgbldir=mumps.gld	// current global directory
		 *	 gtmj3=/dev/null	// input file parameter to job command
		 *	 gtmj4=x.mjo		// output file parameter to job command
		 *	 gtmj5=x.mje		// error file parameter to job command
		 *	 gtmj7=x		// routine name to job off
		 *	 gtmj8=			// label name to job off
		 *	 gtmj9=0		// offset to job off
		 *	 gtmja=			// base priority;
		 *	 gtmjb=			// startup parameter to job command
		 *	 gtmj000=1		// parameter 1 to routine ^x
		 *	 gtmj001=2		// parameter 2 to routine ^x
		 *	 gtmjcnt=2		// number of parameters to routine ^x
		 *	 env1=one		// old environment
		 *	 env2=two		// old environment
		 *
		 *	those parameters that are NULL or 0 are not passed.
		 *	each line above is an entry in the environment array.
		 */

		env_ind = env_ary = (char **)malloc((environ_count + MAX_JOB_QUALS + argcnt + 1)*SIZEOF(char *));

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

		string_len = STRLEN("%s=%d") + STRLEN(CHILD_FLAG_ENV) + MAX_NUM_LEN - 4;
		if (string_len > MAX_JOB_LEN)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
		c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
		SPRINTF_ENV_NUM(c1, CHILD_FLAG_ENV, par_pid, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		/* Pass all information about the job via shell's environment.
		 * The grandchild will get those variables to obtain the info about the job.
		 */

		/* pass global directory to child */
		if (jparms->gbldir.len != 0)
		{
			if (jparms->gbldir.len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->gbldir.addr, jparms->gbldir.len);
			*(pbuff + jparms->gbldir.len) = '\0';
			string_len = STRLEN("%s=%s") + STRLEN(GBLDIR_ENV) + STRLEN(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(c1, GBLDIR_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}
		/* pass startup program to child */
		if (jparms->startup.len != 0)
		{
			if (jparms->startup.len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->startup.addr, jparms->startup.len);
			*(pbuff + jparms->startup.len) = '\0';
			string_len = STRLEN("%s=%s") + STRLEN(STARTUP_ENV) + STRLEN(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(c1, STARTUP_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}
		/* pass input file to child */
		if (jparms->input.len != 0)
		{
			if (jparms->input.len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->input.addr, jparms->input.len);
			*(pbuff + jparms->input.len) = '\0';
			string_len = STRLEN("%s=%s") + STRLEN(IN_FILE_ENV) + STRLEN(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(c1, IN_FILE_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}

		/* pass output file to child */
		if (jparms->output.addr != 0)
		{
			if (jparms->output.len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->output.addr, jparms->output.len);
			*(pbuff + jparms->output.len) = '\0';
			if (jobpid)
				SPRINTF(&pbuff[jparms->output.len], ".%d", getpid());
			string_len = STRLEN("%s=%s") + STRLEN(OUT_FILE_ENV) + STRLEN(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(c1, OUT_FILE_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}

		/* pass error file to child */
		if (jparms->error.len != 0)
		{
			if (jparms->error.len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->error.addr, jparms->error.len);
			*(pbuff + jparms->error.len) = '\0';
			if (jobpid)
				SPRINTF(&pbuff[jparms->error.len], ".%d", getpid());
			string_len = STRLEN("%s=%s") + STRLEN(ERR_FILE_ENV) + STRLEN(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(c1, ERR_FILE_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}

		/* pass routine name to child */
		if (jparms->routine.len != 0)
		{
			if (jparms->routine.len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			strncpy(pbuff, jparms->routine.addr, jparms->routine.len);
			*(pbuff + jparms->routine.len) = '\0';
			string_len = STRLEN("%s=%s") + STRLEN(ROUTINE_ENV) + STRLEN(pbuff) - 4;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_STR(c1, ROUTINE_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}

		/* pass label name to child */
		if (jparms->label.len > TEMP_BUFF_SIZE)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
		strncpy(pbuff, jparms->label.addr, jparms->label.len);
		*(pbuff + jparms->label.len) = '\0';
		string_len = STRLEN("%s=%s") + STRLEN(LABEL_ENV) + STRLEN(pbuff) - 4;
		if (string_len > TEMP_BUFF_SIZE)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
		c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
		SPRINTF_ENV_STR(c1, LABEL_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif

		/* pass the offset */
		string_len = STRLEN("%s=%ld") + STRLEN(OFFSET_ENV) + MAX_NUM_LEN - 5;
		if (string_len > TEMP_BUFF_SIZE)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
		c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
		SPRINTF_ENV_NUM(c1, OFFSET_ENV, jparms->offset, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif

		/* pass Priority to child */
		if (jparms->baspri != 0)
		{
			string_len = STRLEN("%s=%ld") + STRLEN(PRIORITY_ENV) + MAX_NUM_LEN - 5;
			if (string_len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SPRINTF_ENV_NUM(c1, PRIORITY_ENV, jparms->baspri, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}

		for (index = 0, jp = jparms->parms;  jp ;  index++, jp = jp->next)
		{
			if (jp->parm->str.len > MAX_JOB_LEN - 2)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			if (0 != jp->parm->mvtype)
			{
				MV_FORCE_STR(jp->parm);
				string_len = STRLEN(PARM_STR) + jp->parm->str.len + 1;
				if (string_len > MAX_JOB_LEN)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
				c1 = (char *)malloc(string_len);
#			ifdef KEEP_zOS_EBCDIC
				__getEstring1_a_copy(c1, STR_AND_LEN(PARM_STRING));
				__getEstring1_a_copy(c1 + strlen(PARM_STRING), jp->parm->str.addr, jp->parm->str.len);
#			else
				SPRINTF(c1, GTMJ_FMT, index);
				memcpy(c1 + strlen(PARM_STR), jp->parm->str.addr, jp->parm->str.len);
#			endif
				*(c1 + string_len - 1) = 0;
				*env_ind++ = c1;
			}
		}
		string_len = STRLEN("%s=%ld") + STRLEN(GTMJCNT_ENV) + MAX_NUM_LEN - 5;
		if (string_len > TEMP_BUFF_SIZE)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
		c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
		SPRINTF_ENV_NUM(c1, GTMJCNT_ENV, index, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif

#ifdef	__osf__
/* Make sure SIZEOF(char *) is correct.  */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif
		/* before appending the old environment into the environment array, do not add those
		 * lines that correspond to any of the above already initialized environment variables.
		 * this prevents indefinite growing of the environment array with nesting of job commands
		 * which otherwise would show up eventually as an "Arg list too long" error from EXECVE() below.
		 */
		new_env_top = env_ind;
		old_env_top = &environ[environ_count];
		DEBUG_ONLY(
			/* check that all new environment variables begin with the string "gtm".
			 * this assumption is used later in the for loop below.
			 */
			for (new_env_cur = env_ary; new_env_cur < new_env_top; new_env_cur++)
				assert(!STRNCMP_LIT(*new_env_cur, "gtm"));
		)
		for (old_env_cur = environ; old_env_cur < old_env_top; old_env_cur++)
		{
			env_end = strchr(*old_env_cur, '=');
			if ((NULL != env_end) && !STRNCMP_LIT(*old_env_cur, "gtm"))
			{
				env_len = (int)(env_end - *old_env_cur + 1);	/* include the '=' too */
				assert(env_len <= strlen(*old_env_cur));
				for (new_env_cur = env_ary; new_env_cur < new_env_top; new_env_cur++)
				{
					if (0 == strncmp(*new_env_cur, *old_env_cur, env_len))
						break;
				}
				if (new_env_cur < new_env_top)
					continue;
			}
			*env_ind++ = *old_env_cur;
		}
		*env_ind = NULL;	/* null terminator required by execve() */

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

		c1 = gtm_dist;
		string_len = STRLEN(c1);
		if ((string_len + SIZEOF(MUMPS_EXE_STR)) < SIZEOF(tbuff))
		{
			memcpy(tbuff, c1, string_len);
			c2 = &tbuff[string_len];
			strcpy(c2, MUMPS_EXE_STR);
		} else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, string_len, c1,
				SIZEOF(tbuff) - SIZEOF(MUMPS_EXE_STR));

#		ifdef KEEP_zOS_EBCDIC_	/* use real strcpy to preserve env in native code set */
#		pragma convlit(suspend)
#		endif
		strcpy(cbuff, MUMPS_DIRECT_STR);
#		ifdef KEEP_zOS_EBCDIC_
#		pragma convlit(resume)
#		endif

		/* pass cmdline to child */
		if (jparms->cmdline.len != 0)
		{
			if (jparms->cmdline.len > TEMP_BUFF_SIZE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBPARTOOLONG);
			memcpy(cmdbuff, jparms->cmdline.addr, jparms->cmdline.len);
			*(cmdbuff + jparms->cmdline.len) = 0;
		} else
			memset(cmdbuff, 0, TEMP_BUFF_SIZE);

#ifdef KEEP_zOS_EBCDIC
		__getEstring1_a_copy(tbuff2, STR_AND_LEN(tbuff));
		argv[0] = tbuff2;
#else
		argv[0] = tbuff;
#endif
		argv[1] = cbuff;
		argv[2] = cmdbuff;
		argv[3] = (char *)0;

		EXECVE(tbuff, argv, env_ary);
		/* if we got here, error starting the Job */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_LIT("Exec error in Job"), errno);
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
		{	/* note : macro, WAITPID would not be suitable here because we want to catch our timeout
			 * waitpid expects an integer wait_status even for _BSD cases, but WIF* macros expect
			 * a union wait argument (on AIX)
			 */
			done_pid = waitpid(child_pid, &wait_status, 0);	/* BYPASSOK */
		} while(!ojtimeout && 0 > done_pid && EINTR == errno);
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
			} else if (-1 != kill_ret && done_pid == child_pid)
				return (wait_status);	/* timer popped in the window of child fork and middle process exit */
			*non_exit_return = TRUE;
			return TIMEOUT_ERROR;	/* return special value so as to eliminate the window where the timer
						 * might pop after this routine returns to the callee and before the callee
						 * analyses the return status (ojtimeout may not be a reliable indicator) */
		} else if (0 > done_pid) /* Some other signal received OR there is no child to be waited on */
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

	return (EINVAL); /* This should never get executed, added to make compiler happy */
}
