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

#ifndef __JOBSP_H__
#define __JOBSP_H__

#define MAX_JOBPAR_LEN		255
#define MAX_FILSPC_LEN		255
#define MAX_PIDSTR_LEN		10
#define MAX_MBXNAM_LEN		16
#define MAX_PRCNAM_LEN		15
#define MAX_STDIOE_LEN		1024
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

GBLDEF int job_errno;

/********************************************************************************************************************
 * Following enum is used to identify the cause of error in the middle process (M) to the main thread (P)
 * during the startup of a Job. (passed to the parent (P) through the exit status).
 * The last two enums are special. They MUST be the last two for any new additions in the future.
 * The last one identifies the end of the enum list. Last but one (joberr_tryagain) is used to identify
 * the situations where trying again by the main thread might succeed (like errors due to insufficient
 * swap space etc..). When the middle process comes across an error that it thinks is worth trying again,
 * it adds joberr_tryagain to the main status (one of the status' upto joberr_tryagain) and exits with the new status.
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
	joberr_frk,
	joberr_stdout_rename,
	joberr_stderr_rename,
	joberr_pipe_mp,
	joberr_pipe_mgc,
	joberr_stp,
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
	mstr		logfile;
	mstr		routine;
	mstr		label;
	mstr		cmdline;
	int4		baspri;
	int4		offset;
	job_parm	*parms;
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

int ojchildioset(job_params_type *jparms);
int ojstartchild(job_params_type *jparms, int argcnt, boolean_t *non_exit_return, int pipe_fds[]);
void ojparams(char *p, job_params_type *job_params);
void ojgetch_env(job_params_type *jparms);
void ojchildioclean(void);

#endif
