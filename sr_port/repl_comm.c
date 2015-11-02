/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtmio.h"

#include <sys/types.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
#include <sys/poll.h>
#endif

#include "repl_msg.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "repl_comm.h"
#include "repl_sp.h"
#include "min_max.h"
#include "rel_quant.h"
#include "repl_log.h"
#include "iotcpdef.h"
#include "gtmmsg.h"
#include "gt_timer.h"
/* These statistics are useful and should perhaps be collected - Vinaya 2003/08/18
 *
 * Common:
 *
 * Send:
 * # calls to repl_send
 * # calls to repl_send with unaligned buffer
 * # bytes repl_send called with, distribute into buckets
 * % of input bytes repl_send actually sent, or # bytes actually sent distributed into buckets
 * # calls to select, and timeout distributed into buckets
 * # calls to send
 * # calls to select that were interrupted (EINTR)
 * # calls to select that were unsuccessful due to system resource shortage (EAGAIN)
 * # calls to select that timed out
 * # calls to send that were interrupted (EINTR)
 * # calls to send that failed due to the message size being too big (EMSGSIZE)
 * # calls to send that would have blocked (EWOULDBLOCK)
 *
 * Receive:
 * # calls to repl_recv
 * # calls to repl_recv with unaligned buffer
 * # bytes repl_recv called with, distribute into buckets
 * % of input length repl_recv actually received, or # bytes actuall received distributed into buckets
 * # calls to select, and timeout distributed into buckets
 * # calls to recv
 * # calls to select that were interrupted (EINTR)
 * # calls to select that were unsuccessful due to system resource shortage (EAGAIN)
 * # calls to select that timed out
 * # calls to recv that were interrupted (EINTR)
 * # calls to recv that failed due to the connection reset (bytes received == 0)
 * # calls to recv that would have blocked (EWOULDBLOCK)
 */

GBLDEF	int	repl_max_send_buffsize, repl_max_recv_buffsize;
#if defined(__hppa) || defined(__vms)
#define REPL_SEND_TRACE_BUFF_SIZE 65536
#define REPL_RECV_TRACE_BUFF_SIZE 65536
#else
#define REPL_SEND_TRACE_BUFF_SIZE 1048576
#define REPL_RECV_TRACE_BUFF_SIZE 1048576
#endif
#define REPL_SEND_SIZE_TRACE_SIZE 1024
#define REPL_RECV_SIZE_TRACE_SIZE 1024
STATICDEF int repl_send_trace_buff_pos = 0;
STATICDEF unsigned char * repl_send_trace_buff = 0;
STATICDEF int repl_send_size_trace_pos = 0;
STATICDEF int repl_send_size_trace[REPL_SEND_SIZE_TRACE_SIZE];
STATICDEF int repl_recv_trace_buff_pos = 0;
STATICDEF unsigned char * repl_recv_trace_buff = 0;
STATICDEF int repl_recv_size_trace_pos = 0;
STATICDEF int repl_recv_size_trace[REPL_RECV_SIZE_TRACE_SIZE];

int repl_send(int sock_fd, unsigned char *buff, int *send_len, boolean_t skip_pipe_ready_check, struct timeval *max_pipe_ready_wait)
{ /* On entry, *send_len is the number of bytes to be sent
   * On exit, *send_len contains the number of bytes sent
   */
	int		send_size, status, eintr_cnt, eagain_cnt, ewouldblock_cnt, emsgsize_cnt;
  	ssize_t		bytes_sent;
	long		wait_val;
	fd_set		output_fds;
        struct timeval	timeout;
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
	long		poll_timeout;
	unsigned long	poll_nfds;
	struct pollfd	poll_fdlist[1];
#endif
	int		space_to_end;

	if (!repl_send_trace_buff)
		repl_send_trace_buff = malloc(REPL_SEND_TRACE_BUFF_SIZE);
	/* Note: there is no corresponding free for this malloc since it is only done once per process and will not
	 * accumulate across multiple process invocations. It will be "freed" when the mupip process exits.
	 */
	send_size = *send_len;
	/* VMS returns SYSTEM-F-INVBUFLEN if send_size is larger than the hard limit VMS_MAX_TCP_SEND_SIZE (64K - 1 on some
	 * impelementations, 64K - 512 on some others). VMS_MAX_TCP_SEND_SIZE may be larger than repl_max_send_buffsize, and
	 * empirically we have noticed send() successfully sending repl_max_send_buffsize or more bytes.
	 */
	VMS_ONLY(send_size = MIN(send_size, VMS_MAX_TCP_SEND_SIZE);)
	REPL_DPRINT3("repl_send: called with send_size %d, limiting send_size to %d\n", *send_len, send_size);
	*send_len = 0;
	if (!skip_pipe_ready_check)
	{ /* Wait till connection pipe is ready for sending */
		assert(max_pipe_ready_wait->tv_sec  == 0); /* all callers pass sub-second timeout. We take advantage of this fact
							    * to avoid division while computing wait_val */
		assert(max_pipe_ready_wait->tv_usec >= 0 && max_pipe_ready_wait->tv_usec < MICROSEC_IN_SEC);
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
		FD_ZERO(&output_fds);
		FD_SET(sock_fd, &output_fds);
#else
        	poll_fdlist[0].fd = sock_fd;
        	poll_fdlist[0].events = POLLOUT;
        	poll_nfds = 1;
        	poll_timeout = max_pipe_ready_wait->tv_usec / 1000;   /* convert to millisecs */
#endif
		/* the check for EINTR below is valid and should not be converted to an EINTR wrapper macro, because EAGAIN is also
		 * being checked */
		for (timeout = *max_pipe_ready_wait, eintr_cnt = eagain_cnt = 0;
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
			-1 == (status = select(sock_fd + 1, NULL, &output_fds, NULL, &timeout))
#else
        		-1 == (status = poll(&poll_fdlist[0], poll_nfds, poll_timeout))
#endif
		     && (EINTR == errno || EAGAIN == errno); )
		{
			if (EINTR == errno)
			{
				wait_val = (0 == eintr_cnt) ?  (max_pipe_ready_wait->tv_usec >> 1) : (wait_val >> 1);
				if (++eintr_cnt > REPL_COMM_MAX_INTR_CNT)
				{ /* assume timeout and give up. Note, this may result in a sleep different from intended.
				   * But, we can live with it as there is no impact on the user */
					status = 0;
					break;
				}
				timeout.tv_sec  = 0;
				timeout.tv_usec = (gtm_tv_usec_t)wait_val;
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
				poll_timeout = wait_val / 1000;		/* convert to millisecs */
#endif
				REPL_DPRINT5("repl_send: select interrupted, changing timeout from tv_sec %ld tv_usec %ld to "
						"tv_sec %ld tv_usec %ld\n", 0, (wait_val << 1), timeout.tv_sec, timeout.tv_usec);
			} else
			{ /* resource starved system; relinquish the processor in the hope that we may get the required
			   * resources the next time around */
				if (0 == ++eagain_cnt % REPL_COMM_LOG_EAGAIN_INTERVAL)
					repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: System appears to be "
						 "resource starved; EAGAIN returned from repl_send/select %d times\n", eagain_cnt);
				rel_quant();
			}
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
			FD_SET(sock_fd, &output_fds);	/* Linux/gcc does not like this in the iterator */
#endif
		}
	} else
		status = 1; /* assume connection pipe is ready for sending */
	if (0 < status)
	{ /* Ready for sending */
		/*
		 * the check for EINTR below is valid and should not be converted to an EINTR wrapper macro, because other errno
		 * values are being checked.
		 */
		/* Trace last REPL_SEND_SIZE_TRACE_SIZE sizes of what was sent */
		repl_send_size_trace[repl_send_size_trace_pos++] = send_size;
		repl_send_size_trace_pos %= ARRAYSIZE(repl_send_size_trace);
		/* Trace last REPL_SEND_TRACE_BUFF_SIZE bytes sent. */
		if (send_size > REPL_SEND_TRACE_BUFF_SIZE)
		{
			/* if the message size > our buffer just copy the last our buffer-size-worth starting from the
			 * beginning of our buffer and reset pos to the beginning of our buffer.
			 */
			memcpy(repl_send_trace_buff, buff + send_size - REPL_SEND_TRACE_BUFF_SIZE, REPL_SEND_TRACE_BUFF_SIZE);
			repl_send_trace_buff_pos = 0;
		}
		else
		{
			space_to_end = REPL_SEND_TRACE_BUFF_SIZE - repl_send_trace_buff_pos;
			if (send_size > space_to_end)
			{
				memcpy(repl_send_trace_buff + repl_send_trace_buff_pos, buff, space_to_end);
				memcpy(repl_send_trace_buff, buff + space_to_end, send_size - space_to_end);
			}
			else
			{
				memcpy(repl_send_trace_buff + repl_send_trace_buff_pos, buff, send_size);
			}
			repl_send_trace_buff_pos = (repl_send_trace_buff_pos + send_size) % REPL_SEND_TRACE_BUFF_SIZE;
		}
		if (0 < send_size)
		{
			for (ewouldblock_cnt = emsgsize_cnt = 0;
				(((bytes_sent = send(sock_fd, (char *)buff, send_size, 0)) < 0)
					&& (EINTR == errno || EMSGSIZE == errno || EWOULDBLOCK == errno)); )
			{
				if (EINTR == errno)
					continue;
				assert(EMSGSIZE != errno); /* since we use blocking sockets, we don't expect to see EMSGSIZE */
				if (EMSGSIZE == errno)
				{ /* Reduce the send size if possible */
					if (send_size > REPL_COMM_MIN_SEND_SIZE)
					{
						if ((send_size >> 1) <= REPL_COMM_MIN_SEND_SIZE)
							send_size = REPL_COMM_MIN_SEND_SIZE;
						else
							send_size >>= 1;
					}
					if (0 == ++emsgsize_cnt % REPL_COMM_LOG_EMSGSIZE_INTERVAL)
						repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: System "
								"appears to be clogged; EMSGSIZE returned from send "
								"%d times\n", emsgsize_cnt);
				} else
				{
					if (0 == ++ewouldblock_cnt % REPL_COMM_LOG_EWDBLCK_INTERVAL)
						repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: System "
								"appears to be running slow; EWOULDBLOCK returned from "
								"send %d times\n", ewouldblock_cnt);
					rel_quant(); /* we hope the cause for blocking would have cleared by the time
							* we get scheduled next time around */
				}
			}
		} else
			bytes_sent = 0;
		if (0 <= bytes_sent)
		{
			*send_len = (int)bytes_sent;
			REPL_DPRINT2("repl_send: returning with send_len %ld\n", bytes_sent);
			return (SS_NORMAL);
		}
		repl_errno = EREPL_SEND;
		return (ERRNO);
	} else if (0 == status)
		return (SS_NORMAL);
	repl_errno = EREPL_SELECT;
	return (ERRNO);
}

int repl_recv(int sock_fd, unsigned char *buff, int *recv_len, boolean_t skip_data_avail_check, struct timeval *max_data_avail_wait)
{ /* On entry *recv_len is the maximum length to be received.
   * On exit, *recv_len will contain the number of bytes received.
   */
	int		status, max_recv_len, eintr_cnt, eagain_cnt;
	ssize_t		bytes_recvd;
	long		wait_val;
	fd_set		input_fds;
        struct timeval	timeout;
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
	long		poll_timeout;
	unsigned long	poll_nfds;
	struct pollfd	poll_fdlist[1];
#endif
	int		space_to_end;

	if (!repl_recv_trace_buff)
		repl_recv_trace_buff = malloc(REPL_RECV_TRACE_BUFF_SIZE);
	/* Note: there is no corresponding free for this malloc since it is only done once per process and will not
	 * accumulate across multiple process invocations. It will be "freed" when the mupip process exits.
	 */
	assert(FD_INVALID != sock_fd);
	max_recv_len = *recv_len;
	/* VMS returns SYSTEM-F-INVBUFLEN if max_recv_len is larger than the hard limit VMS_MAX_TCP_SEND_SIZE (64K - 1 on some
	 * impelementations, 64K - 512 on some others). Although VMS_MAX_TCP_RECV_SIZE may be larger than repl_max_recv_buffsize,
	 * we have empirically noticed recv() returning with repl_max_recv_buffsize or fewer bytes.
	 */
	VMS_ONLY(max_recv_len = MIN(max_recv_len, VMS_MAX_TCP_RECV_SIZE);)
	REPL_DPRINT3("repl_recv: called with max_recv_len %d, limiting max_recv_len to %d\n", *recv_len, max_recv_len);
	*recv_len = 0;
	if (!skip_data_avail_check)
	{
		assert(max_data_avail_wait->tv_sec  == 0); /* all callers pass sub-second timeout. We take advantage of this fact
							    * to avoid division while computing wait_val */
		assert(max_data_avail_wait->tv_usec >= 0 && max_data_avail_wait->tv_usec < MICROSEC_IN_SEC);
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
		FD_ZERO(&input_fds);
		FD_SET(sock_fd, &input_fds);
#else
        	poll_fdlist[0].fd = sock_fd;
        	poll_fdlist[0].events = POLLIN;
        	poll_nfds = 1;
        	poll_timeout = max_data_avail_wait->tv_usec / 1000;   /* convert to millisecs */
#endif
		/* the check for EINTR below is valid and should not be converted to an EINTR wrapper macro, because EAGAIN is also
		 * being checked */
		for (timeout = *max_data_avail_wait, eintr_cnt = eagain_cnt = 0;
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
		     -1 == (status = select(sock_fd + 1, &input_fds, NULL, NULL, &timeout))
#else
        		-1 == (status = poll(&poll_fdlist[0], poll_nfds, poll_timeout))
#endif
		     && (EINTR == errno || EAGAIN == errno); )
		{
			if (EINTR == errno)
			{
				wait_val = (0 == eintr_cnt) ? (max_data_avail_wait->tv_usec >> 1) : (wait_val >> 1);
				if (++eintr_cnt > REPL_COMM_MAX_INTR_CNT)
				{ /* assume timeout and give up. Note, this may result in a sleep different from intended.
				   * But, we can live with it as there is no impact on the user */
					status = 0;
					break;
				}
				timeout.tv_sec  = 0;
				timeout.tv_usec = (gtm_tv_usec_t)wait_val;
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
				poll_timeout = wait_val / 1000;		/* convert to millisecs */
#endif
				REPL_DPRINT5("repl_recv: select interrupted, changing timeout from tv_sec %ld tv_usec %ld to "
						"tv_sec %ld tv_usec %ld\n", 0, (wait_val << 1), timeout.tv_sec, timeout.tv_usec);
			} else
			{ /* resource starved system; relinquish the processor in the hope that we may get the required
			   * resources the next time around */
				if (0 == ++eagain_cnt % REPL_COMM_LOG_EAGAIN_INTERVAL)
					repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: System appears to be "
							"resource starved; EAGAIN returned from select %d times\n", eagain_cnt);
				rel_quant();
			}
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
			FD_SET(sock_fd, &input_fds);	/* Linux/gcc does not like this in the iterator */
#endif
		}
	} else
		status = 1; /* assume data is available, consequence is recv() may block if data is not available */
	if (0 < status)
	{ /* Data available on the pipe */
		while (0 > (bytes_recvd = recv(sock_fd, (char *)buff, max_recv_len, 0)) && EINTR == errno)
			;
		if (0 < bytes_recvd)
		{
			*recv_len = (int)bytes_recvd;
			REPL_DPRINT2("repl_recv: returning with recv_len %ld\n", bytes_recvd);
			/* Trace last REPL_RECV_SIZE_TRACE_SIZE sizes of what was received */
			repl_recv_size_trace[repl_recv_size_trace_pos++] = bytes_recvd;
			repl_recv_size_trace_pos %= ARRAYSIZE(repl_recv_size_trace);
			/* Trace last REPL_RECV_TRACE_BUFF_SIZE bytes received. */
			if (bytes_recvd > REPL_RECV_TRACE_BUFF_SIZE)
			{
				/* if the message size > our buffer just copy the last our buffer-size-worth starting from the
				 *  beginning of our buffer and reset pos to the beginning of our buffer.
				 */
				memcpy(repl_recv_trace_buff, buff + bytes_recvd - REPL_RECV_TRACE_BUFF_SIZE,
						REPL_RECV_TRACE_BUFF_SIZE);
				repl_recv_trace_buff_pos = 0;
			}
			else
			{
				space_to_end = REPL_RECV_TRACE_BUFF_SIZE - repl_recv_trace_buff_pos;
				if (bytes_recvd > space_to_end)
				{
					memcpy(repl_recv_trace_buff + repl_recv_trace_buff_pos, buff, space_to_end);
					memcpy(repl_recv_trace_buff, buff + space_to_end, bytes_recvd - space_to_end);
				}
				else
				{
					memcpy(repl_recv_trace_buff + repl_recv_trace_buff_pos, buff, bytes_recvd);
				}
				repl_recv_trace_buff_pos = (repl_recv_trace_buff_pos + bytes_recvd) %
						REPL_RECV_TRACE_BUFF_SIZE;
			}
			return (SS_NORMAL);
		}
		if (0 == bytes_recvd)
		{ /* Connection reset */
			errno = ECONNRESET;
		} else if (EWOULDBLOCK == errno)
		{ /* NOTE: Although we use non blocking sockets, it is possible to get EWOULDBLOCK error status if receive
		   * timeout has been set and the timeout expired before data was received (from man recv on RH 8 Linux). Some
		   * systems return ETIMEDOUT for the timeout condition. */
			repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: network I/O failed to complete; "
					"socket recv() returned EWOULDBLOCK\n");
			errno = ETIMEDOUT; /* will be treated as a bad connection and the connection closed */
		}
		repl_errno = EREPL_RECV;
		return (ERRNO);
	} else if (0 == status)
		return (SS_NORMAL);
	repl_errno = EREPL_SELECT;
	return (ERRNO);
}

int repl_close(int *sock_fd)
{
	int	status = 0;

	if (FD_INVALID != *sock_fd)
		CLOSEFILE_RESET(*sock_fd, status);	/* resets "*sock_fd" to FD_INVALID */
	return (0 == status ? 0 : ERRNO);
}

static int get_sock_buff_size(int sockfd, int *buffsize, int which_buf)
{
	int	status;
	GTM_SOCKLEN_TYPE optlen;

	optlen = SIZEOF(*buffsize);
        status = getsockopt(sockfd, SOL_SOCKET, which_buf, (void *)buffsize, (GTM_SOCKLEN_TYPE *)&optlen);
	return (0 == status) ? 0 : ERRNO;
}

int get_send_sock_buff_size(int sockfd, int *send_buffsize)
{
	return get_sock_buff_size(sockfd, send_buffsize, SO_SNDBUF);
}

int get_recv_sock_buff_size(int sockfd, int *recv_buffsize)
{
	return get_sock_buff_size(sockfd, recv_buffsize, SO_RCVBUF);
}

static int set_sock_buff_size(int sockfd, int buflen, int which_buf)
{
	int	status;
#ifndef sun
	size_t	optlen;
#else
	int	optlen;
#endif
	optlen = SIZEOF(buflen);
        status = setsockopt(sockfd, SOL_SOCKET, which_buf, (void *)&buflen, (GTM_SOCKLEN_TYPE)optlen);
	return (0 == status) ? 0 : ERRNO;
}

int set_send_sock_buff_size(int sockfd, int buflen)
{
	return set_sock_buff_size(sockfd, buflen, SO_SNDBUF);
}

int set_recv_sock_buff_size(int sockfd, int buflen)
{
	return set_sock_buff_size(sockfd, buflen, SO_RCVBUF);
}

void repl_log_conn_info(int sock_fd, FILE *log_fp)
{
	struct sockaddr_in	local, remote;
	GTM_SOCKLEN_TYPE	len;
	int			save_errno;
        size_t			errlen;
	char			*errptr, local_ip[16], remote_ip[16];
	in_port_t		local_port, remote_port;
	error_def(ERR_GETSOCKNAMERR);
	error_def(ERR_TEXT);

	len = SIZEOF(local);
	if (0 == getsockname(sock_fd, (struct sockaddr *)&local, (GTM_SOCKLEN_TYPE *)&len))
	{
		local_port = ntohs(local.sin_port);
		strcpy(local_ip, inet_ntoa(local.sin_addr));
	} else
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		errlen = strlen(errptr);
		gtm_putmsg(VARLSTCNT(9) ERR_GETSOCKNAMERR, 3, save_errno, errlen, errptr, ERR_TEXT, 2,
				LEN_AND_LIT("LOCAL"));

		local_port = (unsigned short)-1;
		strcpy(local_ip, "*UNKNOWN*");
	}
	len = SIZEOF(remote);
	if (0 == getpeername(sock_fd, (struct sockaddr *)&remote, (GTM_SOCKLEN_TYPE *)&len))
	{
		remote_port = ntohs(remote.sin_port);
		strcpy(remote_ip, inet_ntoa(remote.sin_addr));
	} else
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		errlen = strlen(errptr);
		gtm_putmsg(VARLSTCNT(9) ERR_GETSOCKNAMERR, 3, save_errno, errlen, errptr, ERR_TEXT, 2,
				LEN_AND_LIT("REMOTE"));
		remote_port = (unsigned short)-1;
		strcpy(remote_ip, "*UNKNOWN*");
	}
	repl_log(log_fp, TRUE, TRUE, "Connection information:: Local: %s:%d Remote: %s:%d\n", local_ip, local_port,
			remote_ip, remote_port);
	return;
}
