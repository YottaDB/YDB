/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_FD_TRACE

#include <sys/types.h>
#include <errno.h>

/* Before including gtm_fcntl.h, make sure we do not redefine open/creat/close etc. as this module
 * is where we are defining the interlude functions gtm_open/gtm_creat etc. to use the real system version.
 * Note that if the system include file redefined open/creat etc. (e.g. HPUX redefines open to __open64 to allow
 * for large file support) we want to make sure we use the system redefined versions here. So it is necessary
 * to disable any redefining of these functions that GT.M otherwise does.
 */
#undef	GTM_FD_TRACE

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_socket.h"
#include "caller_id.h"
#include "iosp.h"
#include "error.h"
#include "gtm_string.h"
#include "send_msg.h"

/* This is a GT.M wrapper module for all system calls that open/close file descriptors.
 * This is needed to trace all files that were opened by GT.M (D9I11-002714)
 */

enum fd_ops
{
	fd_ops_open = 1,	/* open   */
	fd_ops_open3,		/* open   */
	fd_ops_creat,		/* creat  */
	fd_ops_dup,		/* dup    */
	fd_ops_dup2,		/* dup2   */
	fd_ops_pipe0, 		/* pipe   */
	fd_ops_pipe1, 		/* pipe   */
	fd_ops_socket, 		/* socket */
	fd_ops_close		/* close  */
};

typedef struct
{
	caddr_t		call_from;
	int		fd;
	enum fd_ops	fd_act;
	int4		status;	/* valid only if fd_act is fd_ops_close or fd_ops_pipe{0,1} */
} fd_trace;

#define	FD_OPS_ARRAY_SIZE	512

GBLDEF	int4		fd_ops_array_index = -1;
GBLDEF	int4		fd_ops_array_num_wraps = 0;		/* to get an idea how many total files were opened/closed */
GBLDEF	fd_trace	fd_ops_array[FD_OPS_ARRAY_SIZE];	/* space for FD_TRACE macro to record info */

error_def(ERR_CLOSEFAIL);
error_def(ERR_CALLERID);

/* Determine on what platforms and build types we want to get caller_id information. In pro builds, invoke caller_id only on
 * those platforms where caller_id is lightweight (i.e. caller_id.s exists). Thankfully AIX (necessary for D9I11-002714)
 * falls in this category. For debug builds, invoke caller_id unconditionally since performance is not a big concern.
 */
#if (defined(DEBUG) || defined(_AIX) || defined(__sparc) || defined(__MVS__) || defined(Linux390)	\
		|| (defined(__linux__) && (defined(__i386))) || defined(__osf__))
#	define	GET_CALLER_ID	caller_id()
#else
#	define	GET_CALLER_ID	0
#endif

#define	FD_TRACE(OPS, FD, STATUS)						\
{										\
	++fd_ops_array_index;							\
	if (FD_OPS_ARRAY_SIZE <= fd_ops_array_index)				\
	{									\
		fd_ops_array_num_wraps++;					\
		fd_ops_array_index = 0;						\
	}									\
	fd_ops_array[fd_ops_array_index].call_from = (caddr_t)GET_CALLER_ID;	\
	fd_ops_array[fd_ops_array_index].fd_act    = OPS;			\
	fd_ops_array[fd_ops_array_index].fd        = FD;			\
	fd_ops_array[fd_ops_array_index].status    = (int4)STATUS;		\
}

int gtm_open(const char *pathname, int flags)
{
	int	fd;

	fd = open(pathname, flags);
	FD_TRACE(fd_ops_open, fd, 0);
	return fd;
}

int gtm_open3(const char *pathname, int flags, mode_t mode)
{
	int	fd;

	fd = open(pathname, flags, mode);
	FD_TRACE(fd_ops_open3, fd, 0);
	return fd;
}

int gtm_creat(const char *pathname, mode_t mode)
{
	int	fd;

	fd = creat(pathname, mode);
	FD_TRACE(fd_ops_creat, fd, 0);
	return fd;
}

int gtm_dup(int oldfd)
{
	int	newfd;

	newfd = dup(oldfd);
	FD_TRACE(fd_ops_dup, newfd, oldfd);
	assert(-1 != newfd);
	return newfd;
}

int gtm_dup2(int oldfd, int newfd)
{
	int	status;

	status = dup2(oldfd, newfd);
	assert((-1 == status) || (newfd == status));
	FD_TRACE(fd_ops_dup2, newfd, (-1 == status) ? status : oldfd);
	assert(-1 != newfd);
	return newfd;
}

int gtm_pipe1(int pipefd[2])
{
	int	status;

	status = pipe(pipefd);
	FD_TRACE(fd_ops_pipe0, pipefd[0], status);
	FD_TRACE(fd_ops_pipe1, pipefd[1], status);
	assert(-1 != status);
	return status;
}

int gtm_socket(int family, int type, int protocol)
{
	int	fd;

	fd = socket(family, type, protocol);
	if (-1 != fd)
		FD_TRACE(fd_ops_socket, fd, 0);
	/* it is possible that fd will be -1 if the address family is not supported */
	return fd;
}

int gtm_close(int fd)
{
	int	status;
	int	save_errno;

	status = close(fd);
	save_errno = errno;
	FD_TRACE(fd_ops_close, fd, status);
	if ((-1 == status) && (EINTR != save_errno))
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLOSEFAIL, 1, fd, save_errno);
		SEND_CALLERID("gtm_close()");
		assert(FALSE);
	}
	return status;
}

#endif
