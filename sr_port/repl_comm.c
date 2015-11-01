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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "repl_msg.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "repl_comm.h"
#include "repl_sp.h"

GBLDEF	uint4	repl_max_send_buffsize, repl_max_recv_buffsize;

int repl_send(int sock_fd, unsigned char *buff,
	      int *send_len, struct timeval *timeout)
{
	/*
	 * On entry, *send_len is the length to be sent.
	 * On exit, *send_len will contain the number of bytes sent
	 */
	int 	bytes_sent, send_size, status;
	fd_set	output_fds;
        struct timeval save_timeout;

	assert(0 < repl_max_send_buffsize); /* Make sure that repl_max_send_buffsize is inited (through get_sock_buff_size) */
	send_size = (*send_len < repl_max_send_buffsize) ? *send_len : repl_max_send_buffsize;
	assert(1 <= send_size);
	*send_len = 0;

	/* Wait till connection pipe is ready for sending */

	FD_ZERO(&output_fds);
	FD_SET(sock_fd, &output_fds);

	/*
	 * the check for EINTR below is valid and should not be converted to an EINTR
	 * wrapper macro, because EAGAIN is also being checked.
	 */

	/* May want to add some wait and a counter if EAGAIN */
	for ( save_timeout = *timeout;
     	     0 > (status = select(sock_fd + 1, NULL, &output_fds, NULL, timeout))
	     && (EINTR == errno || EAGAIN == errno); *timeout = save_timeout)
             FD_SET(sock_fd, &output_fds);	/* Linux/gcc does not like this in the iterator */
        *timeout = save_timeout;

	if (0 < status)
	{
		/*
		 * the check for EINTR below is valid and should not be converted to an EINTR
		 * wrapper macro, because other errno values are being checked.
		 */

		/* Ready for sending */
		while (0 < send_size && (bytes_sent = send(sock_fd, (char *) buff, send_size, 0)) < 0
				     && (EINTR == errno || EMSGSIZE == errno || EWOULDBLOCK == errno))
		{
			if (EINTR == errno)
				continue;
			/* Attempt half the send_size */
			send_size = (((send_size >> 1) > 1) ? (send_size >> 1) : 1);
		}

		if (0 < bytes_sent)
		{
			*send_len = bytes_sent;
			return (SS_NORMAL);
		} else
		{
			repl_errno = EREPL_SEND;
			return (ERRNO);
		}
	} else if (0 == status)
		return (SS_NORMAL);

	repl_errno = EREPL_SELECT;
	return (ERRNO);
}

int repl_recv(int sock_fd, unsigned char *buff,
	      int *recv_len, struct timeval *timeout)
{
	/* Use this when the expected length is known */

	/*
	 * On entry *recv_len is the length to be received.
	 * On exit, *recv_len will contain the number of bytes received
	 */

	int	bytes_recvd;
	fd_set	input_fds;
	int	status;
	int	save_recv_len;
        struct timeval save_timeout;

	FD_ZERO(&input_fds);
	FD_SET(sock_fd, &input_fds);

	assert(0 < repl_max_recv_buffsize); /* Make sure that repl_max_recv_buffsize is inited (through get_sock_buff_size) */
	save_recv_len = (*recv_len < repl_max_recv_buffsize) ? *recv_len : repl_max_recv_buffsize;
	*recv_len = 0;

	/*
	 * the check for EINTR below is valid and should not be converted to an EINTR
	 * wrapper macro, because EAGAIN is also being checked.
	 */

	/* May want to add some wait and a counter if EAGAIN */
	for ( save_timeout = *timeout;
	     0 > (status = select(sock_fd + 1, &input_fds, NULL, NULL, timeout)) && (EINTR == errno || EAGAIN == errno);
	      *timeout = save_timeout)
	      FD_SET(sock_fd, &input_fds);	/* Linux/gcc doesn't like this in the iterator */
        *timeout = save_timeout;
	if (0 < status)
	{
		/* Something to receive */
		while (0 > ((bytes_recvd = recv(sock_fd, (char *) buff, save_recv_len, 0)) && EINTR == errno));

		if (0 < bytes_recvd)
		{
			*recv_len = bytes_recvd;
			return (SS_NORMAL);
		} else if (0 == bytes_recvd) /* Connection reset */
		{
			errno = ECONNRESET;
		}
		assert(EWOULDBLOCK != errno);
		repl_errno = EREPL_RECV;
		return (ERRNO);
	} else if (0 == status)
		return (SS_NORMAL);

	repl_errno = EREPL_SELECT;
	return (ERRNO);
}

int repl_close(int *sock_fd)
{
	close(*sock_fd);
	*sock_fd = -1;
	return (SS_NORMAL);
}

int get_sock_buff_size(int sockfd, uint4 *send_buffsize, uint4 *recv_buffsize)
{
	int status;
#ifndef sun
	size_t	optlen;
#else
	int	optlen;
#endif

	optlen = sizeof(uint4);
        if (0 == (status = getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)send_buffsize, &optlen)))
	{
		optlen = sizeof(uint4);
		status = getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)recv_buffsize, &optlen);
	}
	return status;
}
