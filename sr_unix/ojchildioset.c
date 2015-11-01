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

#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "job.h"
#include "gtm_stat.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "gtmmsg.h"

GBLREF boolean_t	job_try_again;

#define NULL_DEV_FNAME	"/dev/null"	/* Null device file name */
#define OPEN_RSRC_CRUNCH_FAILURE		\
	(EDQUOT == errno || ENFILE == errno || ENOMEM == errno || ENOSPC == errno || ETIMEDOUT == errno)

/*
 * ---------------------------------------------------------
 * Set up input, output and error file descriptors in
 * a child.
 * ---------------------------------------------------------
 */
bool ojchildioset(job_params_type *jparms)
{
	int	dup_ret, in_fd, out_fd, err_fd, save_errno;
	char 	fname_buf[1024], buf[1024];

	error_def(ERR_TEXT);
	error_def(ERR_JOBFAIL);

/*
 * Redirect input
 */
	strncpy(fname_buf, jparms->input.addr, jparms->input.len);
	*(fname_buf + jparms->input.len) = '\0';

	OPENFILE(fname_buf, O_RDONLY, in_fd);
	if (in_fd == -1)
	{
		if (OPEN_RSRC_CRUNCH_FAILURE)
			job_try_again = TRUE;
		else
		{
			save_errno = errno;
			SPRINTF(buf, "Error redirecting stdin (open) to %s", fname_buf);
			gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		}
		return FALSE;
	}

	close(0);
	FCNTL3(in_fd, F_DUPFD, 0, dup_ret);
	if (-1 == dup_ret)
	{
		save_errno = errno;
		SPRINTF(buf, "Error redirecting stdin (fcntl) to %s", fname_buf);
		gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		return FALSE;
	}
	close(in_fd);
/*
 * Redirect Output
 */
	strncpy(fname_buf, jparms->output.addr, jparms->output.len);
	*(fname_buf + jparms->output.len) = '\0';

	CREATE_FILE(fname_buf, 0666, out_fd);
	if (-1 == out_fd)
	{
		if (OPEN_RSRC_CRUNCH_FAILURE)
			job_try_again = TRUE;
		else
		{
			save_errno = errno;
			SPRINTF(buf, "Error redirecting stdout (creat) to %s", fname_buf);
			gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		}
		return FALSE;
	}

	close(out_fd);

	OPENFILE(fname_buf, O_WRONLY, out_fd);
	if (out_fd == -1)
	{
		if (OPEN_RSRC_CRUNCH_FAILURE)
			job_try_again = TRUE;
		else
		{
			save_errno = errno;
			SPRINTF(buf, "Error redirecting stdout (open) to %s", fname_buf);
			gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		}
		return FALSE;
	}

	close(1);
	FCNTL3(out_fd, F_DUPFD, 0, dup_ret);
	if (-1 == dup_ret)
	{
		save_errno = errno;
		SPRINTF(buf, "Error redirecting stdout (fcntl) to %s", fname_buf);
		gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		return FALSE;
	}
	close(out_fd);
/*
 * Redirect Error
 */
	strncpy(fname_buf, jparms->error.addr, jparms->error.len);
	*(fname_buf + jparms->error.len) = '\0';

	CREATE_FILE(fname_buf, 0666, err_fd);
	if (-1 == err_fd)
	{
		if (OPEN_RSRC_CRUNCH_FAILURE)
			job_try_again = TRUE;
		else
		{
			save_errno = errno;
			SPRINTF(buf, "Error redirecting stderr (creat) to %s", fname_buf);
			gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		}
		return FALSE;
	}

	close(err_fd);

	OPENFILE(fname_buf, O_WRONLY, err_fd);
	if (err_fd == -1)
	{
		if (OPEN_RSRC_CRUNCH_FAILURE)
			job_try_again = TRUE;
		else
		{
			save_errno = errno;
			SPRINTF(buf, "Error redirecting stderr (open) to %s", fname_buf);
			gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		}
		return FALSE;
	}

	close(2);
	FCNTL3(err_fd, F_DUPFD, 0, dup_ret);
	if (-1 == dup_ret)
	{
		save_errno = errno;
		SPRINTF(buf, "Error redirecting stderr (fcntl) to %s", fname_buf);
		gtm_putmsg(VARLSTCNT(7) ERR_JOBFAIL, 0, ERR_TEXT, 2, LEN_AND_STR(buf), save_errno);
		return FALSE;
	}
	close(err_fd);

	return(TRUE);
}
