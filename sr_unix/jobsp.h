/****************************************************************
 *								*
 * Copyright (c) 2001, 2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __JOBSP_H__
#define __JOBSP_H__

#define MAX_JOBPAR_LEN		255
#define MAX_FILSPC_LEN		255
#define MAX_PIDSTR_LEN		10
#define MAX_MBXNAM_LEN		16
#define MAX_PRCNAM_LEN		15
#define MAX_STDIOE_LEN		1024
#define MAX_JOBPARM_LEN		1024
#define MAX_JOB_LEN		8192	/* The length of the buffer used for storing each argument of the job command */

#define TIMEOUT_ERROR		(MAX_SYSERR + 1)	/* a special value to differentiate it from the rest of errno's */

#define CHILD_FLAG_ENV		"gtmj0"
#define CLEAR_CHILD_FLAG_ENV  	"gtmj0="""
#define GBLDIR_ENV		"gtmgbldir"
#define CWD_ENV			"gtmj2"
#define IN_FILE_ENV		"gtmj3"
#define OUT_FILE_ENV		"gtmj4"
#define ERR_FILE_ENV		"gtmj5"
#define LOG_FILE_ENV		"gtmj6"
#define ROUTINE_ENV		"gtmj7"
#define LABEL_ENV		"gtmj8"
#define OFFSET_ENV		"gtmj9"
#define PRIORITY_ENV		"gtmja"
#define STARTUP_ENV		"gtmjb"
#define GTMJCNT_ENV		"gtmjcnt"

#define JOB_SOCKET_PREFIX		"SOCKET:"
#define IS_JOB_SOCKET(ADDR, LEN)	((LEN >= SIZEOF(JOB_SOCKET_PREFIX)) && (0 == STRNCMP_LIT(ADDR, JOB_SOCKET_PREFIX)))
#define JOB_SOCKET_HANDLE(ADDR)		(((char *)(ADDR)) + SIZEOF(JOB_SOCKET_PREFIX) - 1)
#define JOB_SOCKET_HANDLE_LEN(LEN)	(LEN - SIZEOF(JOB_SOCKET_PREFIX) + 1)

GBLDEF int job_errno;

/********************************************************************************************************************
 * Following enum is used to identify the cause of error in the middle process (M) to the main thread (P)
 * during the startup of a Job. (passed to the parent (P) through the exit status).
 * The last two enums are special. They MUST be the last two for any new additions in the future.
 * The last one identifies the end of the enum list. Last but one (joberr_tryagain) is used to identify
 * the situations where trying again by the main thread might succeed (like errors due to insufficient
 * swap space etc..). When the middle process comes across an error that it thinks is worth trying again,
 * it adds joberr_tryagain to the main status (one of the status' upto joberr_tryagain) and exits with the new status.
 *
 * Additions to this enum must match the joberrs array in joberr.h.
 *********************************************************************************************************************/

typedef enum
{	joberr_ok,
	joberr_gen,
	joberr_io_stdin_open,
	joberr_io_stdin_dup,
	joberr_io_stdout_creat,
	joberr_io_stdout_open,
	joberr_io_stdout_dup,
	joberr_io_stderr_creat,
	joberr_io_stderr_open,
	joberr_io_stderr_dup,
	joberr_cd_toolong,
	joberr_cd,
	joberr_rtn,
	joberr_sid,
	joberr_sp,
	joberr_frk,
	joberr_stdout_rename,
	joberr_stderr_rename,
	joberr_pipe_mp,
	joberr_pipe_mgc,
	joberr_stdin_socket_lookup,
	joberr_stdout_socket_lookup,
	joberr_stderr_socket_lookup,
	joberr_io_stdin_socket_dup,
	joberr_io_stdout_socket_dup,
	joberr_io_stderr_socket_dup,
	joberr_io_setup_op_write,
	joberr_io_setup_write,
	joberr_stp,			/* These three should stay at the end of the enum. */
	joberr_sig,
	joberr_end
} joberr_t;

typedef struct job_parm_struct
{	mval		*parm;
	struct job_parm_struct	*next;
}job_parm;

typedef	struct
{
	mstr		input, output, error;
	mstr		gbldir, startup, directory;
	mstr		routine;
	mstr		label;
	mstr		cmdline;
	int4		baspri;
	int4		offset;
	job_parm	*parms;
	size_t		input_prebuffer_size;
	char		*input_prebuffer;
	boolean_t	passcurlvn;
} job_params_type;

typedef enum
{
	jpdt_nul,
	jpdt_num,
	jpdt_str
} jp_datatype;

#define JPDEF(a,b) a
typedef enum
{
#include "jobparams.h"
} jp_type;

typedef enum
{
	job_done,			/* last message */
	job_set_params,			/* followed by a job_params_msg message */
	job_set_parm_list,		/* followed by a job_arg_count_msg and "arg_count" job_arg_msg messages */
	job_set_input_buffer,		/* followed by a job_buffer_size_msg and a data message of "buffer_size" */
	job_set_locals,			/* followed by local_variable */
	local_trans_done		/* Indicates all of the locals have been sent to the grandchild */
} job_setup_op;

typedef struct
{
	size_t		directory_len;
	char		directory[MAX_JOBPARM_LEN];
	size_t		gbldir_len;
	char		gbldir[MAX_JOBPARM_LEN];
	size_t		startup_len;
	char		startup[MAX_JOBPARM_LEN];
	size_t		input_len;
	char		input[MAX_JOBPARM_LEN];
	size_t		output_len;
	char		output[MAX_JOBPARM_LEN];
	size_t		error_len;
	char		error[MAX_JOBPARM_LEN];
	size_t		routine_len;
	char		routine[MAX_JOBPARM_LEN];
	size_t		label_len;
	char		label[MAX_JOBPARM_LEN];
	int		offset;
	int		baspri;
} job_params_msg;

typedef size_t job_arg_count_msg;

typedef struct
{
	ssize_t			len;				/* negative len indicates null arg */
	char			data[MAX_JOB_LEN];
} job_arg_msg;

typedef size_t job_buffer_size_msg;

int ojchildioset(job_params_type *jparms);
int ojstartchild(job_params_type *jparms, int argcnt, boolean_t *non_exit_return, int pipe_fds[]);
void ojparams(char *p, job_params_type *job_params);
void ojgetch_env(job_params_type *jparms);
void ojchildioclean(void);
void ojmidchild_send_var(void);
#endif
