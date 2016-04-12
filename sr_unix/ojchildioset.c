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

#include "mdef.h"

#include <errno.h>

#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"

#include "job.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "gtmmsg.h"
#include "io.h"
#include "iosocketdef.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

GBLREF	int			job_errno;
GBLREF	d_socket_struct		*socket_pool;

ZOS_ONLY(error_def(ERR_BADTAG);)
error_def(ERR_JOBFAIL);
error_def(ERR_TEXT);
/*
 * ---------------------------------------------------------
 * Set up output and error file descriptors in
 * a child.
 * ---------------------------------------------------------
 */
int ojchildioset(job_params_type *jparms)
{
	int		dup_ret, in_fd, out_fd, err_fd;
	char 		fname_buf[MAX_STDIOE_LEN], buf[MAX_STDIOE_LEN];
	int		rc;
	joberr_t	joberr = joberr_gen;
	int4		index;
	mstr_len_t	handle_len;
	socket_struct	*socketptr;
	ZOS_ONLY(int	realfiletag;)

	/*
	 * Open Input
	 */
	if (IS_JOB_SOCKET(jparms->input.addr, jparms->input.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(jparms->input.len);
		index = iosocket_handle(JOB_SOCKET_HANDLE(jparms->input.addr), &handle_len, FALSE, socket_pool);
		if (-1 == index)
		{
			joberr = joberr_stdin_socket_lookup;
			job_errno = EINVAL;
			return joberr;
		}
		socketptr = socket_pool->socket[index];
		FCNTL3(socketptr->sd, F_DUPFD, 1, dup_ret);
		if (-1 == dup_ret)
		{
			joberr = joberr_io_stdin_socket_dup;
			job_errno = errno;
			return joberr;
		}
		in_fd = dup_ret;
		jparms->input_prebuffer_size = socketptr->buffered_length;
		if (0 < jparms->input_prebuffer_size)
		{	/* copy any buffered reads */
			jparms->input_prebuffer = malloc(jparms->input_prebuffer_size);
			memcpy(jparms->input_prebuffer, socketptr->buffer + socketptr->buffered_offset,
				jparms->input_prebuffer_size);
		}
	} else
	{
		strncpy(fname_buf, jparms->input.addr, jparms->input.len);
		*(fname_buf + jparms->input.len) = '\0';

		OPENFILE(fname_buf, O_RDONLY, in_fd);
		if (FD_INVALID == in_fd)
		{
			joberr = joberr_io_stdin_open;
			job_errno = errno;
			return joberr;
		}
		jparms->input_prebuffer_size = 0;
	}

	/*
	 * Open Output
	 */
	if (IS_JOB_SOCKET(jparms->output.addr, jparms->output.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(jparms->output.len);
		index = iosocket_handle(JOB_SOCKET_HANDLE(jparms->output.addr), &handle_len, FALSE, socket_pool);
		if (-1 == index)
		{
			joberr = joberr_stdout_socket_lookup;
			job_errno = EINVAL;
			return joberr;
		}
		FCNTL3(socket_pool->socket[index]->sd, F_DUPFD, 1, dup_ret);
		if (-1 == dup_ret)
		{
			joberr = joberr_io_stdout_socket_dup;
			job_errno = errno;
			return joberr;
		}
		out_fd = dup_ret;
	} else
	{
		strncpy(fname_buf, jparms->output.addr, jparms->output.len);
		*(fname_buf + jparms->output.len) = '\0';

		CREATE_FILE(fname_buf, 0666, out_fd);
		if (FD_INVALID == out_fd)
		{
			joberr = joberr_io_stdout_creat;
			job_errno = errno;
			return joberr;
		}
#		ifdef __MVS__
		/* tagging as ASCII is fine now, that might change in the future for gtm_utf8_mode */
		if (-1 == gtm_zos_set_tag(out_fd, TAG_ASCII, TAG_TEXT, TAG_FORCE, &realfiletag))
			TAG_POLICY_SEND_MSG(fname_buf, errno, realfiletag, TAG_ASCII);
#		endif
		CLOSEFILE_RESET(out_fd, rc);	/* resets "out_fd" to FD_INVALID */

		OPENFILE(fname_buf, O_WRONLY, out_fd);
		if (FD_INVALID == out_fd)
		{
			joberr = joberr_io_stdout_open;
			job_errno = errno;
			return joberr;
		}
	}

	/*
	 * Open Error
	 */
	if (IS_JOB_SOCKET(jparms->error.addr, jparms->error.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(jparms->error.len);
		index = iosocket_handle(JOB_SOCKET_HANDLE(jparms->error.addr), &handle_len, FALSE, socket_pool);
		if (-1 == index)
		{
			joberr = joberr_stderr_socket_lookup;
			job_errno = EINVAL;
			return joberr;
		}
		FCNTL3(socket_pool->socket[index]->sd, F_DUPFD, 1, dup_ret);
		if (-1 == dup_ret)
		{
			joberr = joberr_io_stderr_socket_dup;
			job_errno = errno;
			return joberr;
		}
		err_fd = dup_ret;
	} else
	{
		strncpy(fname_buf, jparms->error.addr, jparms->error.len);
		*(fname_buf + jparms->error.len) = '\0';

		CREATE_FILE(fname_buf, 0666, err_fd);
		if (FD_INVALID == err_fd)
		{
			joberr = joberr_io_stderr_creat;
			job_errno = errno;
			return joberr;
		}
#		ifdef __MVS__
		if (-1 == gtm_zos_set_tag(err_fd, TAG_EBCDIC, TAG_TEXT, TAG_FORCE, &realfiletag))
			TAG_POLICY_SEND_MSG(fname_buf, errno, realfiletag, TAG_EBCDIC);
#		endif
		CLOSEFILE_RESET(err_fd, rc);	/* resets "err_fd" to FD_INVALID */
		OPENFILE(fname_buf, O_WRONLY, err_fd);
		if (FD_INVALID == err_fd)
		{
			joberr = joberr_io_stderr_open;
			job_errno = errno;
			return joberr;
		}
	}

	/*
	 * Redirect Input
	 */
	CLOSEFILE(0, rc);
	FCNTL3(in_fd, F_DUPFD, 0, dup_ret);
	if (-1 == dup_ret)
	{
		joberr = joberr_io_stdin_dup;
		job_errno = errno;
		return joberr;
	}
#	ifdef __MVS__
	/* policy tagging because by default input is /dev/null */
	if (-1 == gtm_zos_tag_to_policy(in_fd, TAG_UNTAGGED, &realfiletag))
		TAG_POLICY_SEND_MSG(fname_buf, errno, realfiletag, TAG_UNTAGGED);
#	endif
	CLOSEFILE_RESET(in_fd, rc);	/* resets "in_fd" to FD_INVALID */

	/*
	 * Redirect Output
	 */
	CLOSEFILE(1, rc);
	FCNTL3(out_fd, F_DUPFD, 0, dup_ret);
	if (-1 == dup_ret)
	{
		joberr = joberr_io_stdout_dup;
		job_errno = errno;
		return joberr;
	}
	CLOSEFILE_RESET(out_fd, rc);	/* resets "out_fd" to FD_INVALID */

	/*
	 * Redirect Error
	 */
	CLOSEFILE(2, rc);
	FCNTL3(err_fd, F_DUPFD, 0, dup_ret);
	if (-1 == dup_ret)
	{
		joberr = joberr_io_stderr_dup;
		job_errno = errno;
		return joberr;
	}
	CLOSEFILE_RESET(err_fd, rc);	/* resets "err_fd" to FD_INVALID */

	return 0;
}
