/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JOBSP_H_INCLUDED
#define JOBSP_H_INCLUDED

#include "gtm_stdio.h"

#include "gtm_signal.h"

#define MAX_JOBPAR_LEN		255
#define MAX_FILSPC_LEN		255
#define MAX_PIDSTR_LEN		10
#define MAX_MBXNAM_LEN		16
#define MAX_PRCNAM_LEN		15
#define MAX_STDIOE_LEN		1024
#define MAX_JOBPARM_LEN		1024
#define MAX_JOB_LEN		8192	/* The length of the buffer used for storing each argument of the job command */

#define TIMEOUT_ERROR		(MAX_SYSERR + 1)	/* a special value to differentiate it from the rest of errno's */

#define CHILD_FLAG_ENV		"ydb_j0"
#define CLEAR_CHILD_FLAG_ENV  	"ydb_j0="""
#define GBLDIR_ENV		(ydbenvname[YDBENVINDX_GBLDIR] + 1)	/* + 1 to skip leading $ in env var name */

#define JOB_SOCKET_PREFIX		"SOCKET:"
#define IS_JOB_SOCKET(ADDR, LEN)	((LEN >= SIZEOF(JOB_SOCKET_PREFIX)) && (0 == STRNCMP_LIT(ADDR, JOB_SOCKET_PREFIX)))
#define JOB_SOCKET_HANDLE(ADDR)		(((char *)(ADDR)) + SIZEOF(JOB_SOCKET_PREFIX) - 1)
#define JOB_SOCKET_HANDLE_LEN(LEN)	(LEN - SIZEOF(JOB_SOCKET_PREFIX) + 1)

GBLREF int job_errno;

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
{
#	define	JOBERR_TABLE_ENTRY(JOBERR_ENUM, JOBERR_STRING)	JOBERR_ENUM,
#	include "joberr_table.h"
#	undef JOBERR_TABLE_ENTRY
} joberr_t;

typedef struct job_parm_struct
{
	mval			*parm;
	struct job_parm_struct	*next;
} job_parm;

typedef struct job_param_str_struct
{
	size_t		len;
	char		buffer[MAX_JOBPARM_LEN];
} job_param_str;

struct job_params_struct
{
	job_param_str	directory;
	job_param_str	gbldir;
	job_param_str	startup;
	job_param_str	input;
	job_param_str	output;
	job_param_str	error;
	job_param_str	routine;
	job_param_str	label;
	int		offset;
	int		baspri;
};

typedef	struct
{
	struct job_params_struct	params;
	job_param_str			cmdline;
	job_parm			*parms;
	size_t				input_prebuffer_size;
	char				*input_prebuffer;
	boolean_t			passcurlvn;
	char				*curlvn_buffer_ptr;
	size_t				curlvn_buffer_size;
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

typedef struct job_params_struct	job_params_msg;
typedef size_t				job_arg_count_msg;

typedef struct
{
	ssize_t			len;				/* negative len indicates null arg */
	char			data[MAX_JOB_LEN];
} job_arg_msg;

typedef size_t job_buffer_size_msg;

int ojchildioset(job_params_type *jparms);
int ojstartchild(job_params_type *jparms, int argcnt, boolean_t *non_exit_return, int pipe_fds[]);
void ojparams(char *p, job_params_type *job_params);
void ojchildioclean(void);
void ojpassvar_hook(void);
void local_variable_marshalling(FILE *output);
void job_term_handler(int sig, siginfo_t *info, void *context);

#endif
