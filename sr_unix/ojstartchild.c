/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "gtm_signal.h"
#include "gtm_fcntl.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_limits.h"

#include <errno.h>
#include <sys/wait.h>

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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gtmio.h"
#include "fork_init.h"
#include "rtnobj.h"
#include "getjobnum.h"
#include "zshow.h"
#include "zwrite.h"
#include "gtm_maxstr.h"
#include "getzdir.h"
#ifdef DEBUG
#include <sys/time.h>
#include <sys/resource.h>
#endif

GBLREF	bool			jobpid;	/* job's output files should have the pid appended to them. */
GBLREF	volatile boolean_t	ojtimeout;
GBLREF  boolean_t       	gtm_pipe_child;
GBLREF	int			job_errno;
GBLREF	zshow_out		*zwr_output;
GBLREF	uint4			pat_everything[];
GBLREF	mstr_len_t		sizeof_pat_everything;
GBLREF	io_pair			io_curr_device;
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		skip_exit_handler;

static  joberr_t		joberr = joberr_gen;
static	int			pipe_fd;
static	int			setup_fds[2];	/* socket pair for setup of final mumps process */
static	FILE			*setup_file;
static	pid_t			child_pid;

#ifndef SYS_ERRLIST_INCLUDE
/* currently either stdio.h or errno.h both of which are included above needed by TIMEOUT_ERROR in jobsp.h */
#if !defined(__sun) && !defined(___MVS__)
GBLREF	int			sys_nerr;
#endif
#endif

GBLREF char		**environ;
GBLREF char		gtm_dist[GTM_PATH_MAX];
GBLREF boolean_t	gtm_dist_ok_to_use;
LITREF gtmImageName	gtmImageNames[];

#define MAX_MUMPS_EXE_PATH_LEN	8192
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
		/* reap the just killed child, so there won't be any zombies */		\
		WAITPID(PROCESS_ID, &wait_status, 0, done_pid);				\
		assert(done_pid == PROCESS_ID);						\
	}										\
}

#define FORK_RETRY(PID)						\
{								\
	FORK(PID);						\
	while (-1 == PID)					\
	{							\
		assertpro(EAGAIN == errno || ENOMEM == errno);	\
		usleep(50000);					\
		FORK(PID);					\
	}							\
}

#define SETUP_OP_FAIL()											\
{													\
	kill(child_pid, SIGTERM);									\
	joberr = joberr_io_setup_op_write;								\
	job_errno = errno;										\
	DOWRITERC(pipe_fd, &job_errno, SIZEOF(job_errno), pipe_status);					\
	assert(FALSE);											\
	UNDERSCORE_EXIT(joberr);									\
}

#define SETUP_DATA_FAIL()										\
{													\
	kill(child_pid, SIGTERM);									\
	joberr = joberr_io_setup_write;									\
	job_errno = errno;										\
	DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);				\
	assert(FALSE);											\
	UNDERSCORE_EXIT(joberr);									\
}

error_def(ERR_GTMDISTUNVERIF);
error_def(ERR_JOBFAIL);
error_def(ERR_JOBLVN2LONG);
error_def(ERR_JOBPARTOOLONG);
error_def(ERR_LOGTOOLONG);
error_def(ERR_TEXT);

/* Note that this module uses _exit instead of exit to avoid running the inherited exit handlers which this mid-level process does
 * not want to run.
 */

STATICFNDCL void job_term_handler(int sig);
STATICFNDCL int io_rename(job_params_msg *params, const int jobid);

/* Middle process sets the entryref for the jobbed-off process by calling job_addr(). job_addr() internally calls op_zlink(). If
 * rts_error occurs when we are in the job_addr() function, the topmost condition handler inherited from parent process will be
 * executed. That condition handler might not be adequate to handle the rts_error happened in the context of middle process.
 * Hence middle process is provided with its own condition handler.
 */
static CONDITION_HANDLER(middle_child)
{
	int pipe_status;

	START_CH(FALSE);
	/* As of now, middle process could encounter rts_error only while setting entryref. Hence the following assert.
	 * Do assert after write to prevent parent hang.
	 */
	assert(joberr == joberr_rtn);
	DOWRITERC(pipe_fd, &job_errno, SIZEOF(job_errno), pipe_status);
	UNDERSCORE_EXIT(joberr);
}

/* Following condition handles the rts_error occurred in the grandchild before doing execv(). Since we have not started executing
 * M-cmd specified as a part of JOB command, it is enough to print the error and exit.
 */
static CONDITION_HANDLER(grand_child)
{
	PRN_ERROR;
	UNDERSCORE_EXIT(EXIT_SUCCESS);
}

/* Clean up zwrite state if it threw an error in ojpassvar_hook() */
static CONDITION_HANDLER(ojlvzwr_ch)
{
	START_CH(FALSE);

	TREF(in_zwrite) = FALSE;
	setup_file = NULL;
	TREF(zwrite_output_hook) = NULL;
	maxstr_stack_level--;

	NEXTCH;
}

/* This is to close the window of race condition where the timeout occurs and actually by that time, the middle process had already
 * successfully forked-off the job.
 */
STATICFNDEF void job_term_handler(int sig)
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
		else if (0 > ret)
		{
			if (job_errno == ECHILD)
				exit_status = joberr_sig;
			else
				assert(FALSE);
			UNDERSCORE_EXIT(exit_status);
		} else
			return;
}

/* This function does not update params->output and params->error when it renames those files. This is fine for now since nothing
 * else after this point uses those JOB command parameters. */
STATICFNDEF int io_rename(job_params_msg *params, const int jobid)
{
	char path[MAX_STDIOE_LEN];

	STRNCPY_STR(path, params->output.buffer, params->output.len);
	SNPRINTF(&path[params->output.len], MAX_STDIOE_LEN - params->output.len, ".%d", jobid);
	path[sizeof(path) - 1] = '\0';
	if (rename(params->output.buffer, path))
	{
		job_errno = errno;
		return(joberr_stdout_rename);
	}
	/* When OUTPUT and ERROR both point to the same file, rename it once */
	if ((params->output.len == params->error.len) &&
		(0 == STRNCMP_STR(params->output.buffer, params->error.buffer, params->output.len)))
		return 0;
	STRNCPY_STR(path, params->error.buffer, params->error.len);
	SNPRINTF(&path[params->error.len], MAX_STDIOE_LEN - params->error.len,  ".%d", jobid);
	path[sizeof(path) - 1] = '\0';
	if (rename(params->error.buffer, path))
	{
		job_errno = errno;
		return(joberr_stderr_rename);
	}
	return 0;
}

/* For use as a zwrite_output_hook to convert each formatted local into a message and send it to the setup_file. */
void ojpassvar_hook(void)
{
	job_setup_op		setup_op;
	int			rc, pipe_status;
	job_buffer_size_msg	buffer_size;
	size_t			written;

	buffer_size = zwr_output->ptr - zwr_output->buff;
	if (buffer_size == 0)
		/* We can end up here if lvzwr_var()->lvzwr_out() sends the entire buffer so there's nothing left to send from
		 * lvzwr_var()
		 */
		return;
	setup_op = job_set_locals;
	rc = gtm_fwrite(&setup_op, 1, SIZEOF(setup_op), setup_file, &written);
	if (0 < rc)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fwrite(op)"), CALLFROM, errno, 0);
	/* Crop the ' ;*' string at the end of the aliases */
	if (zwr_output->buff[buffer_size - 1] == '*' && zwr_output->buff[buffer_size - 2] == ';'
	    && zwr_output->buff[buffer_size - 3] == ' ')
		buffer_size -= 3;
	/* Always send the buffer size. If it is bigger than MAX_STRLEN, the child will handle this as a JOBLVN2LONG */
	rc = gtm_fwrite(&buffer_size, 1, SIZEOF(buffer_size), setup_file, &written);
	if (0 < rc)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fwrite(size)"), CALLFROM, errno, 0);
	if (buffer_size > MAX_STRLEN)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_JOBLVN2LONG, 2, MAX_STRLEN, buffer_size);
	rc = gtm_fwrite(zwr_output->buff, 1, buffer_size, setup_file, &written);
	if (0 < rc)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fwrite(buf)"), CALLFROM, errno, 0);
	zwr_output->ptr = zwr_output->buff;
	return;
}

void local_variable_marshalling(FILE *setup_file_orig)
{
	/* Setup buffer to write local variables one at a time */
	zshow_out	output;
	mval		pattern;
	char		buffer[MAX_STRLEN];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	memset(&output, 0, SIZEOF(output));
	output.code = 'V';
	output.type = ZSHOW_BUFF_ONLY;
	/* Expanded MAXSTR_BUFF_DECL/MAXSTR_BUFF_INIT below because they are designed to handle the size of MAX_STRBUFF_INIT when we
	 * really want is MAX_STRLEN The condition handler established within MAXSTR_BUFF_INIT is futile because any error we
	 * encounter here is fatal
	 */
	output.buff = buffer;
	output.size = MAX_STRLEN;
	maxstr_stack_level++;
	assert(maxstr_stack_level < MAXSTR_STACK_SIZE);
	maxstr_buff[maxstr_stack_level].len = output.size;
	maxstr_buff[maxstr_stack_level].addr = NULL;
	output.ptr = output.buff;
	pattern.str.addr = (char *)pat_everything;
	pattern.str.len = sizeof_pat_everything;
	pattern.mvtype = MV_STR;
	TREF(zwrite_output_hook) = (void *) &ojpassvar_hook;
	setup_file = setup_file_orig;

	ESTABLISH(ojlvzwr_ch);
	lvzwr_init(zwr_patrn_mval, &pattern);
	lvzwr_fini(&output, ZWRITE_END);
	REVERT;

	/* Keep the following in sync with ojlvzwr_ch() */
	TREF(in_zwrite) = FALSE;
	setup_file = NULL;
	TREF(zwrite_output_hook) = NULL;
	maxstr_stack_level--;
	return;
}

/* --------------------------------------------------------------------------------------------------------------------------------
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
	char			tbuff[MAX_MUMPS_EXE_PATH_LEN], tbuff2[MAX_MUMPS_EXE_PATH_LEN], fname_buf[MAX_STDIOE_LEN];
	char			*pgbldir_str;
	char			*transfer_addr;
	int4			index, environ_count, string_len, temp;
	int			wait_status, save_errno, kill_ret;
	int			rc, dup_ret, in_fd;
	int			status;
	pid_t			done_pid;
	job_parm		*jp;
	rhdtyp			*rtnhdr;
	struct sigaction	act, old_act;
	int			pipe_status, env_len;
	int			mproc_fds[2]; 	/* pipe between middle process and grandchild process */
	int			decision;
	job_setup_op		setup_op;
	job_params_msg		params;
	job_arg_count_msg	arg_count;
	job_arg_msg		arg_msg;
	job_buffer_size_msg	buffer_size;
	char			*c1, *c2, **c3;
	char			*argv[4];
	char			**env_ary, **env_ind;
	char			**new_env_cur, **new_env_top, **old_env_cur, **old_env_top, *env_end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Do the fork and exec but BEFORE that do a FFLUSH(NULL) to make sure any fclose (done in io_rundown
	 * in the child process) does not affect file offsets in this (parent) process' file descriptors
	 */
	if (!gtm_dist_ok_to_use)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_GTMDISTUNVERIF, 4, STRLEN(gtm_dist), gtm_dist,
			gtmImageNames[image_type].imageNameLen, gtmImageNames[image_type].imageName);
	FFLUSH(NULL);
	FORK_RETRY(child_pid);
	if (child_pid == 0)
	{
		/* This is a child process (middle process, M)
		 * Test out various parameters and setup everything possible for the actual Job (J), so it(J) can start off without
		 * much hitch. If any error occurs during this, exit with appropriate status so the waiting parent can diagnose.
		 */
		getjobnum();	/* set "process_id" to a value different from parent. This is particularly needed for the
				 * RELINKCTL_RUNDOWN_MIDDLE_PARENT macro since it does rtnobj_shm_free which deals with latches
				 * that in turn rely on the "process_id" global variable reflecting the current pid.
				 */
		skip_exit_handler = TRUE; /* The grandchild and the middle child should never execute gtm_exit_handler() */
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
			assert(FALSE);
			UNDERSCORE_EXIT(joberr);
		}
		if (!IS_JOB_SOCKET(jparms->params.input.buffer, jparms->params.input.len))
		{
			assert(MAX_STDIOE_LEN > jparms->params.input.len);
			/* Redirect input before potentially changing the default directory below.*/
			strncpy(fname_buf, jparms->params.input.buffer, jparms->params.input.len);
			*(fname_buf + jparms->params.input.len) = '\0';

			OPENFILE(fname_buf, O_RDONLY, in_fd);
			if (FD_INVALID == in_fd)
			{
				joberr = joberr_io_stdin_open;
				job_errno = errno;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
				assert(FALSE);
				UNDERSCORE_EXIT(joberr);
			}
			CLOSEFILE(0, rc);
			FCNTL3(in_fd, F_DUPFD, 0, dup_ret);
			if (-1 == dup_ret)
			{
				joberr = joberr_io_stdin_dup;
				job_errno = errno;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
				assert(FALSE);
				UNDERSCORE_EXIT(joberr);
			}
#			ifdef __MVS__
			/* policy tagging because by default input is /dev/null */
			if (-1 == gtm_zos_tag_to_policy(in_fd, TAG_UNTAGGED, &realfiletag))
				TAG_POLICY_SEND_MSG(fname_buf, errno, realfiletag, TAG_UNTAGGED);
#			endif
			CLOSEFILE_RESET(in_fd, rc);	/* resets "in_fd" to FD_INVALID */
		}
		if (0 != jparms->params.directory.len)
		{
			/* If directory is specified, change it */
			if (jparms->params.directory.len >= TEMP_BUFF_SIZE)
			{
				joberr = joberr_cd_toolong;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
				assert(FALSE);
				UNDERSCORE_EXIT(joberr);
			}
			strncpy(pbuff, jparms->params.directory.buffer, jparms->params.directory.len);
			*(pbuff + jparms->params.directory.len) = '\0';

			if (CHDIR(pbuff) != 0)
			{
				joberr = joberr_cd;
				job_errno = errno;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(errno), pipe_status);
				UNDERSCORE_EXIT(joberr);
			} else	/* update dollar_zdir with the new current working directory */
				getzdir();
		}

		/* attempt to open output files. This also redirects stdin/out/err, so any error messages by this process during the
		 * creation of the Job will get redirected.
		 */
		if ((status = ojchildioset(jparms)))
		{
			DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
			UNDERSCORE_EXIT(status);
		}
		joberr = joberr_sid;
		if (-1 == setsid())
		{
			job_errno = errno;
			DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
			assert(FALSE);
			UNDERSCORE_EXIT(joberr);
		}
#		ifdef DEBUG
		if (WBTEST_ENABLED(WBTEST_JOBFAIL_FILE_LIM))
		{
			const struct rlimit	flim = {2, 2};

			if (-1 == setrlimit(RLIMIT_NOFILE, &flim))
			{	/* the white box test will fail bacause the error reported won't match expectations */
				job_errno = errno;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
				assert(FALSE);	/* core the middle child toward diagnosing why the white box case failed */
			}
		}
#		endif
		joberr = joberr_sp;
		if (-1 == (rc = socketpair(AF_UNIX, SOCK_STREAM, 0, setup_fds)))
		{
			job_errno = errno;
			DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
			assert(WBTEST_ENABLED(WBTEST_JOBFAIL_FILE_LIM));
			UNDERSCORE_EXIT(joberr);
		}
		assert(!WBTEST_ENABLED(WBTEST_JOBFAIL_FILE_LIM));
		/* Kill ourselves before we fork again */
		if (WBTEST_ENABLED(WBTEST_SIGTERM_IN_JOB_CHILD))
			kill(getpid(), SIGTERM);
		/* clone self and exit */
		FORK_RETRY(child_pid);
		if (child_pid)
		{
			/* This is still the middle process.  */
			/* Close the read end of the pipe between middle process and grandchild process. */
			CLOSEFILE_RESET(mproc_fds[0], pipe_status);	/* resets "mproc_fds[0]" to FD_INVALID */
			CLOSEFILE_RESET(setup_fds[1], pipe_status);	/* resets "setup_fds[1]" to FD_INVALID */
			assert(SIZEOF(pid_t) == SIZEOF(child_pid));
			/* params data for 'output' and 'error' is populated here because io_rename() needs it in case appending of
			 * JOB ID to Standard Output and Standard Error is required.
			 */
			params.output.len = jparms->params.output.len;
			memcpy(params.output.buffer, jparms->params.output.buffer, jparms->params.output.len);
			params.output.buffer[jparms->params.output.len] = '\0';
			params.error.len = jparms->params.error.len;
			memcpy(params.error.buffer, jparms->params.error.buffer, jparms->params.error.len);
			params.error.buffer[jparms->params.error.len] = '\0';
			/* if the Job pid need to be appended to the std out/err file names */
			if (jobpid)
			{
				joberr = io_rename(&params, child_pid);
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
					assert(FALSE);
					UNDERSCORE_EXIT(joberr);
				}
			}
			/* Inform grandchild that everything is properly set for it and it is ready to continue. */
			decision = JOB_CONTINUE;
			DOWRITERC(mproc_fds[1], &decision, SIZEOF(decision), pipe_status);
			if (pipe_status)
			{
				kill(child_pid, SIGTERM);
				joberr = joberr_pipe_mgc;
				job_errno = errno;
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
				assert(FALSE);
				UNDERSCORE_EXIT(joberr);
			}
			/* send job parameters and arguments to final mumps process over setup socket */
			setup_op = job_set_params;
			SEND(setup_fds[0], &setup_op, SIZEOF(setup_op), 0, rc);
			if (rc < 0)
				SETUP_OP_FAIL();
			SEND(setup_fds[0], &jparms->params, SIZEOF(jparms->params), 0, rc);
			if (rc < 0)
				SETUP_DATA_FAIL();
			/* Read status to catch any basic errors */
			DOREADRC(setup_fds[0], &joberr, SIZEOF(joberr), rc);
			if (rc < 0)
				joberr = joberr_rtn;	/* Assume routine error if there is a problem getting the report */
			if (joberr_ok != joberr)
			{
				job_errno = (0 < rc) ? rc : joberr; /* non-zero rc is an errno */
				DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
				UNDERSCORE_EXIT(joberr);
			}
			setup_op = job_set_parm_list;
			SEND(setup_fds[0], &setup_op, SIZEOF(setup_op), 0, rc);
			if (rc < 0)
				SETUP_OP_FAIL();
			arg_count = argcnt;
			SEND(setup_fds[0], &arg_count, SIZEOF(arg_count), 0, rc);
			if (rc < 0)
				SETUP_DATA_FAIL();
			for (jp = jparms->parms;  jp ; jp = jp->next)
			{
				if (jp->parm->str.len > MAX_JOB_LEN)
				{
					kill(child_pid, SIGTERM);
					joberr = joberr_io_setup_write;
					job_errno = ERR_JOBPARTOOLONG;
					DOWRITERC(pipe_fds[1], &job_errno, SIZEOF(job_errno), pipe_status);
					assert(FALSE);
					UNDERSCORE_EXIT(joberr);
				}
				if (0 == jp->parm->mvtype)
					arg_msg.len = -1;	/* negative len indicates null arg */
				else
				{
					MV_FORCE_STR(jp->parm);
					arg_msg.len = jp->parm->str.len;
					memcpy(arg_msg.data, jp->parm->str.addr, jp->parm->str.len);
				}
				SEND(setup_fds[0], &arg_msg.len, SIZEOF(arg_msg.len), 0, rc);
				if (rc < 0)
					SETUP_DATA_FAIL();
				if (arg_msg.len >= 0)
				{
					SEND(setup_fds[0], &arg_msg.data, arg_msg.len, 0, rc);
					if (rc < 0)
						SETUP_DATA_FAIL();
				}
			}
			if (0 < jparms->input_prebuffer_size)
			{
				setup_op = job_set_input_buffer;
				SEND(setup_fds[0], &setup_op, SIZEOF(setup_op), 0, rc);
				if (rc < 0)
					SETUP_OP_FAIL();
				buffer_size = jparms->input_prebuffer_size;
				SEND(setup_fds[0], &buffer_size, SIZEOF(buffer_size), 0, rc);
				if (rc < 0)
					SETUP_DATA_FAIL();
				SEND(setup_fds[0], jparms->input_prebuffer, jparms->input_prebuffer_size, 0, rc);
				if (rc < 0)
					SETUP_DATA_FAIL();
				/* input_prebuffer leaks, but the middle process is about to exit, so don't worry about it */
			}
			setup_op = job_done;
			SEND(setup_fds[0], &setup_op, SIZEOF(setup_op), 0, rc);
			if (rc < 0)
				SETUP_OP_FAIL();
			/* Send the local variables */
			if (jparms->xfercurlvn)
			{
				SEND(setup_fds[0], jparms->curlvn_buffer_ptr, jparms->curlvn_buffer_size, 0, rc);
				system_free(jparms->curlvn_buffer_ptr);		/* Space allocated by open_memstream in ojparams */
				if (rc < 0)
					SETUP_OP_FAIL();
			}
			/* Tell job to proceed */
			setup_op = local_trans_done;
			SEND(setup_fds[0], &setup_op, SIZEOF(setup_op), 0, rc);
			if (rc < 0)
				SETUP_OP_FAIL();
			/* Write child_pid into pipe to be read by parent process(P) for $ZJOB */
			/* Ignore the status if this fails, as the child is already running, and there is likely not a parent
			 * to report to.
			 */
			DOWRITERC(pipe_fds[1], &child_pid, SIZEOF(child_pid), pipe_status);
			UNDERSCORE_EXIT(EXIT_SUCCESS);
		}
		/* This is now the grandchild process (actual Job process) -- an orphan as soon as the EXIT(EXIT_SUCCESS) above
		 * occurs. Revert the condition handler established by middle process and establish its own condition handler */
		REVERT;
		getjobnum();	/* set "process_id" to a value different from parent (middle child). This is particularly needed
				 * for the RELINKCTL_RUNDOWN_MIDDLE_PARENT macro since it does rtnobj_shm_free which deals with
				 * latches that in turn rely on the "process_id" global variable reflecting the current pid.
				 */
		ESTABLISH_RET(grand_child, 0);
		sigaction(SIGTERM, &old_act, 0);		/* restore the SIGTERM handler */
		CLOSEFILE_RESET(setup_fds[0], pipe_status);	/* resets "setup_fds[0]" to FD_INVALID */
		/* Since middle child and grand child go off independently, it is possible the grandchild executes
		 * "relinkctl_rundown(TRUE,...)" a little before the middle child has done "relinkctl_rundown(FALSE,...)"
		 * and this means the grand child could potentially find the GT.M nattached counter to be 0 but the shmctl
		 * nattach counter could still be non-zero (because the middle child has not yet detached from relinkctl shm).
		 * To avoid a related assert in "relinkctl_rundown" from failing, set dbg-only variable.
		 */
		DEBUG_ONLY(TREF(fork_without_child_wait) = TRUE);
		DOREADRC(mproc_fds[0], &decision, SIZEOF(decision), pipe_status);
		if (pipe_status)	 /* We failed to read the communication from middle process */
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Error reading from pipe"), errno);
		} else
		{
			if (JOB_EXIT == decision)
				EXIT(EXIT_SUCCESS);
			assert(JOB_CONTINUE == decision);
		}
		CLOSEFILE_RESET(mproc_fds[0], pipe_status);	/* resets "mproc_fds[0]" to FD_INVALID */
		CLOSEFILE_RESET(mproc_fds[1], pipe_status);	/* resets "mproc_fds[1]" to FD_INVALID */
		/* Run down any open flat files to reclaim their file descriptors */
		io_rundown(RUNDOWN_EXCEPT_STD);

		/* release the pipe opened by grand parent (P) */
		CLOSEFILE_RESET(pipe_fds[0], pipe_status);	/* resets "pipe_fds[0]" to FD_INVALID */
		CLOSEFILE_RESET(pipe_fds[1], pipe_status);	/* resets "pipe_fds[1]" to FD_INVALID */

		/* Count the number of environment variables.  */
		for (environ_count = 0, c3 = environ, c2 = *c3;  c2;  c3++, c2 = *c3)
			environ_count++;

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

		env_ind = env_ary = (char **)malloc((environ_count + MAX_JOB_QUALS + 1)*SIZEOF(char *));

		string_len = STRLEN("%s=%d") + STRLEN(CHILD_FLAG_ENV) + MAX_NUM_LEN - 4;
		if (string_len >= MAX_MUMPS_EXE_PATH_LEN)
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_JOBPARTOOLONG);
		}
		c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
		SNPRINTF_ENV_NUM(c1, string_len + 1, CHILD_FLAG_ENV, setup_fds[1], env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		/* Pass all information about the job via shell's environment.
		 * The grandchild will get those variables to obtain the info about the job.
		 */

		/* pass global directory to child */
		if (jparms->params.gbldir.len != 0)
		{
			assert(jparms->params.gbldir.len < TEMP_BUFF_SIZE);
			strncpy(pbuff, jparms->params.gbldir.buffer, jparms->params.gbldir.len);
			*(pbuff + jparms->params.gbldir.len) = '\0';
			string_len = STRLEN("%s=%s") + STRLEN(GBLDIR_ENV) + STRLEN(pbuff) - 4;
			assert(string_len < TEMP_BUFF_SIZE);
			c1 = (char *)malloc(string_len + 1);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
			SNPRINTF_ENV_STR(c1, string_len + 1, GBLDIR_ENV, pbuff, env_ind);
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		}
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

		c1 = gtm_dist;
		string_len = STRLEN(c1);
		if ((string_len + SIZEOF(MUMPS_EXE_STR)) < SIZEOF(tbuff))
		{
			memcpy(tbuff, c1, string_len);
			c2 = &tbuff[string_len];
			strcpy(c2, MUMPS_EXE_STR);
		} else
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_LOGTOOLONG, 3, string_len, c1,
				SIZEOF(tbuff) - SIZEOF(MUMPS_EXE_STR));
		}
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
			if (jparms->cmdline.len >= TEMP_BUFF_SIZE)
			{
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_JOBPARTOOLONG);
			}
			memcpy(cmdbuff, jparms->cmdline.buffer, jparms->cmdline.len);
			*(cmdbuff + jparms->cmdline.len) = 0;
		} else
			memset(cmdbuff, 0, TEMP_BUFF_SIZE);
		/* Do common cleanup in child. Note that the below call to "ojchildioclean" invokes "relinkctl_rundown"
		 * but that is not needed since we have already done it in the RELINKCTL_RUNDOWN_MIDDLE_PARENT invocation.
		 */
		ojchildioclean();

#ifdef KEEP_zOS_EBCDIC
		__getEstring1_a_copy(tbuff2, STR_AND_LEN(tbuff));
		argv[0] = tbuff2;
#else
		argv[0] = tbuff;
#endif
		argv[1] = cbuff;
		argv[2] = cmdbuff;
		argv[3] = (char *)0;
		/* Ignore all SIGHUPs until sig_init() is called. On AIX we have seen SIGHUP from middlechild to grandchild */
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = SIG_IGN;
		sigaction(SIGHUP, &act, 0);	/* downstream process will restore the SIGHUP handler */
		EXECVE(tbuff, argv, env_ary);
		/* if we got here, error starting the Job */
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_LIT("Exec error in Job"), errno);
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
