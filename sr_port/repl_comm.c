/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#ifdef USE_POLL
# include <sys/poll.h>
#endif

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_netdb.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_select.h"
#include "gtm_time.h"
#include "eintr_wrappers.h"

#include "gtmio.h"
#include "repl_msg.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "repl_comm.h"
#include "repl_sp.h"
#include "min_max.h"
#include "rel_quant.h"
#include "repl_log.h"
#include "gtmmsg.h"
#include "gt_timer.h"
#include "have_crit.h"
#ifdef GTM_TLS
#include "error.h"	/* For MAKE_MSG_WARNING macro. */
#include "lv_val.h"
#include "fgncalsp.h"
#include "gtm_tls.h"
#include "gtm_repl.h"
#endif
#ifdef DEBUG
#include "wbox_test_init.h"
#endif

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

GBLDEF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
#ifdef GTM_TLS
GBLREF	gtm_tls_ctx_t		*tls_ctx;
GBLREF	char			dl_err[MAX_ERRSTR_LEN];
#endif

#if defined(__hppa) || defined(__vms)
#define REPL_SEND_TRACE_BUFF_SIZE 65536
#define REPL_RECV_TRACE_BUFF_SIZE 65536
#else
#define REPL_SEND_TRACE_BUFF_SIZE (MAX_REPL_MSGLEN * 2) /* For symmetry with the below */
#define REPL_RECV_TRACE_BUFF_SIZE (MAX_REPL_MSGLEN * 2) /* To debug problems in the Receiver's msgbuff (gtmrecv_msgp) management */
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

error_def(ERR_GETADDRINFO);
error_def(ERR_GETNAMEINFO);
error_def(ERR_GETSOCKNAMERR);
error_def(ERR_TEXT);
error_def(ERR_TLSCLOSE);
error_def(ERR_TLSDLLNOOPEN);
error_def(ERR_TLSINIT);
error_def(ERR_TLSIOERROR);
error_def(ERR_TLSCONNINFO);

#define REPL_TRACE_BUFF(TRACE_BUFF, TRACE_BUFF_POS, IO_BUFF, IO_SIZE, MAX_TRACE_SIZE)			\
{													\
	if (IO_SIZE > MAX_TRACE_SIZE)									\
	{												\
		memcpy(TRACE_BUFF, IO_BUFF + IO_SIZE - MAX_TRACE_SIZE, MAX_TRACE_SIZE);			\
		TRACE_BUFF_POS = 0;									\
	} else												\
	{												\
		int space_to_end = MAX_TRACE_SIZE - TRACE_BUFF_POS;					\
		if (IO_SIZE > space_to_end)								\
		{											\
			memcpy(TRACE_BUFF + TRACE_BUFF_POS, IO_BUFF, space_to_end);			\
			memcpy(TRACE_BUFF, IO_BUFF + space_to_end, IO_SIZE - space_to_end);		\
		} else											\
			memcpy(TRACE_BUFF + TRACE_BUFF_POS, IO_BUFF, IO_SIZE);				\
		TRACE_BUFF_POS = (TRACE_BUFF_POS + IO_SIZE) % MAX_TRACE_SIZE;				\
	}												\
}

int fd_ioready(int sock_fd, int poll_direction, int timeout)
{
	int		save_errno, status, EAGAIN_cnt = 0;
#	ifdef USE_POLL
	struct pollfd	fds;
#	else
	fd_set		fds, *readfds, *writefds;
	struct timeval	timeout_spec;
#	endif

	assert(timeout < MILLISECS_IN_SEC);
	SELECT_ONLY(timeout = timeout * 1000);		/* Convert to microseconds (~ 1sec) */
	assert((timeout >= 0) && (timeout < POLL_ONLY(MILLISECS_IN_SEC) SELECT_ONLY(MICROSECS_IN_SEC)));
#	ifdef USE_POLL
	fds.fd = sock_fd;
	fds.events = (REPL_POLLIN == poll_direction) ? POLLIN : POLLOUT;
#	else
	readfds = writefds = NULL;
	timeout_spec.tv_sec = 0;
	timeout_spec.tv_usec = timeout;
	assertpro(FD_SETSIZE > sock_fd);
	FD_ZERO(&fds);
	FD_SET(sock_fd, &fds);
	writefds = (REPL_POLLOUT == poll_direction) ? &fds : NULL;
	readfds = (REPL_POLLIN == poll_direction) ? &fds : NULL;
#	endif
	POLL_ONLY(while (-1 == (status = poll(&fds, 1, timeout))))
	SELECT_ONLY(while (-1 == (status = select(sock_fd + 1, readfds, writefds, NULL, &timeout_spec))))
	{
		save_errno = ERRNO;
		if (EINTR == save_errno)
		{	/* Give it another shot. But, halve the timeout so we don't keep doing this forever. */
			timeout = timeout >> 1;
		} else if (EAGAIN == save_errno)
		{	/* Resource starved system; relinquish the processor in the hope that we may get the required resources
			 * next time around.
			 */
			if (0 == ++EAGAIN_cnt % REPL_COMM_LOG_EAGAIN_INTERVAL)
			{
				repl_log(stderr, TRUE, TRUE, "Communication subsytem warning: System appears to be resource "
						"starved. EAGAIN returned from select()/poll() %d times\n", EAGAIN_cnt);
			}
			rel_quant();	/* this seems legit */
		} else
			return -1;
		/* Just in case select() modifies the incoming arguments, restore fd_set and timeout_spec */
		SELECT_ONLY(
			assert(0 == timeout_spec.tv_sec);
			timeout_spec.tv_usec = timeout;	/* Note: timeout is the reduced value (in case of EINTR) */
			FD_SET(sock_fd, &fds);
		)
	}
	return status;
}

int repl_send(int sock_fd, unsigned char *buff, int *send_len, int timeout GTMTLS_ONLY_COMMA(int *poll_direction))
{
	int		send_size, status, io_ready, save_errno, EMSGSIZE_cnt = 0, EWOULDBLOCK_cnt = 0;
  	ssize_t		bytes_sent;

	if (!repl_send_trace_buff)
		repl_send_trace_buff = malloc(REPL_SEND_TRACE_BUFF_SIZE);
	/* Note: there is no corresponding free for this malloc since it is only done once per process and will not
	 * accumulate across multiple process invocations. It will be "freed" when the mupip process exits.
	 */
	assert(FD_INVALID != sock_fd);
	send_size = *send_len;
	/* VMS returns SYSTEM-F-INVBUFLEN if send_size is larger than the hard limit VMS_MAX_TCP_SEND_SIZE (64K - 1 on some
	 * impelementations, 64K - 512 on some others). VMS_MAX_TCP_SEND_SIZE may be larger than repl_max_send_buffsize, and
	 * empirically we have noticed send() successfully sending repl_max_send_buffsize or more bytes.
	 */
	VMS_ONLY(send_size = MIN(send_size, VMS_MAX_TCP_SEND_SIZE));
	*send_len = 0;
#	ifdef GTM_TLS
	if (repl_tls.enabled && (REPL_INVALID_POLL_DIRECTION != *poll_direction))
	{
		if (0 >= (io_ready = fd_ioready(sock_fd, *poll_direction, timeout)))
		{
			if (!io_ready)
				return SS_NORMAL;
			save_errno = ERRNO;
			repl_errno = EREPL_SELECT;
			return save_errno;
		}
		/* Fall-through */
	}
#	endif
	if (0 < (io_ready = fd_ioready(sock_fd, REPL_POLLOUT, timeout)))
	{
		/* Trace last REPL_SEND_SIZE_TRACE_SIZE sizes of what was sent */
		repl_send_size_trace[repl_send_size_trace_pos++] = send_size;
		repl_send_size_trace_pos %= ARRAYSIZE(repl_send_size_trace);
		/* Trace last REPL_SEND_TRACE_BUFF_SIZE bytes sent. */
		assert(0 < send_size);
		REPL_TRACE_BUFF(repl_send_trace_buff, repl_send_trace_buff_pos, buff, send_size, REPL_SEND_TRACE_BUFF_SIZE);
		/* The check for EINTR below is valid and should not be converted to an EINTR wrapper macro, because other errno
		 * values are being checked.
		 */
		while (TRUE)
		{
			bytes_sent = GTMTLS_ONLY(repl_tls.enabled ? gtm_tls_send(repl_tls.sock, (char *)buff, send_size)
							: ) send(sock_fd, (char *)buff, send_size, 0);	/* BYPASSOK(send) */
			if (0 <= bytes_sent)
			{
				assert(0 < bytes_sent);
				*send_len = (int)bytes_sent;
				REPL_DPRINT2("repl_send: returning with send_len %ld\n", bytes_sent);
				return SS_NORMAL;
			}
#			ifdef GTM_TLS
			if (repl_tls.enabled && ((GTMTLS_WANT_READ == bytes_sent) || (GTMTLS_WANT_WRITE == bytes_sent)))
			{	/* TLS/SSL implementation couldn't complete the send() without reading or writing more data. Treat
				 * as if nothing was sent. But, mark the poll direction so that then next call to repl_recv waits
				 * for the TCP/IP pipe to be ready for an I/O.
				 */
				*poll_direction = (GTMTLS_WANT_READ == bytes_sent) ? REPL_POLLIN : REPL_POLLOUT;
				return SS_NORMAL;
			}
			/* handle error */
			save_errno = repl_tls.enabled ? gtm_tls_errno() : ERRNO;
			if (-1 == save_errno)
			{	/* This indicates an error from TLS/SSL layer and not from a system call. */
				assert(repl_tls.enabled);
				assert(FALSE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TLSIOERROR, 2, LEN_AND_LIT("send"), ERR_TEXT, 2,
						LEN_AND_STR(gtm_tls_get_error()));
			}
#			else
			save_errno = ERRNO;
#			endif
			assert((EMSGSIZE != save_errno) && (EWOULDBLOCK != save_errno));
			if (EINTR == save_errno)
				continue;
			if (EMSGSIZE == save_errno)
			{
				if (send_size > REPL_COMM_MIN_SEND_SIZE)
				{
					if ((send_size >> 1) <= REPL_COMM_MIN_SEND_SIZE)
						send_size = REPL_COMM_MIN_SEND_SIZE;
					else
						send_size >>= 1;
				}
				if (0 == ++EMSGSIZE_cnt % REPL_COMM_LOG_EMSGSIZE_INTERVAL)
				{
					repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: System appears to be "
							"clogged; EMSGSIZE returned from send %d times\n", EMSGSIZE_cnt);
				}
			} else if (EWOULDBLOCK == save_errno)
			{
				if (0 == ++EWOULDBLOCK_cnt % REPL_COMM_LOG_EWDBLCK_INTERVAL)
				{
					repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: System appears to be "
							"running slow; EWOULDBLOCK returned from send %d times\n", EWOULDBLOCK_cnt);
				}	/* the rel_quant below might be legit */
				rel_quant(); /* Relinquish our quanta in the hope that things get cleared next time around */
			} else
				break;
		}
		repl_errno = EREPL_SEND;
		return save_errno;
	} else if (!io_ready)
		return SS_NORMAL;
	save_errno = ERRNO;
	repl_errno = EREPL_SELECT;
	return save_errno;
}

int repl_recv(int sock_fd, unsigned char *buff, int *recv_len, int timeout GTMTLS_ONLY_COMMA(int *poll_direction))
{
	int		status, max_recv_len, io_ready, save_errno;
	ssize_t		bytes_recvd;

	if (!repl_recv_trace_buff)
		repl_recv_trace_buff = malloc(REPL_RECV_TRACE_BUFF_SIZE);
	/* Note: there is no corresponding free for this malloc since it is only done once per process and will not
	 * accumulate across multiple process invocations. It will be "freed" when the mupip process exits.
	 */
	assert(FD_INVALID != sock_fd);
	max_recv_len = *recv_len;
	/* VMS returns SYSTEM-F-INVBUFLEN if send_size is larger than the hard limit VMS_MAX_TCP_RECV_SIZE (64K - 1 on some
	 * impelementations, 64K - 512 on some others). VMS_MAX_TCP_RECV_SIZE may be larger than repl_max_send_buffsize, and
	 * empirically we have noticed send() successfully sending repl_max_send_buffsize or more bytes.
	 */
	VMS_ONLY(max_recv_len = MIN(max_recv_len, VMS_MAX_TCP_RECV_SIZE));
	*recv_len = 0;
#	ifdef GTM_TLS
	if (repl_tls.enabled && (REPL_INVALID_POLL_DIRECTION != *poll_direction))
	{
		if (0 >= (io_ready = fd_ioready(sock_fd, *poll_direction, timeout)))
		{
			if (!io_ready)
				return SS_NORMAL;
			save_errno = ERRNO;
			repl_errno = EREPL_SELECT;
			return save_errno;
		}
		/* Fall-through */
	}
#	endif
	if ((0 < (io_ready = fd_ioready(sock_fd, REPL_POLLIN, timeout)))
		GTMTLS_ONLY( || (repl_tls.enabled && gtm_tls_cachedbytes(repl_tls.sock))))
	{
		while (TRUE)
		{
			bytes_recvd = GTMTLS_ONLY(repl_tls.enabled ? gtm_tls_recv(repl_tls.sock, (char *)buff, max_recv_len)
							 : ) recv(sock_fd, (char *)buff, max_recv_len, 0);	/* BYPASSOK */
			if (0 < bytes_recvd)
			{
				*recv_len = (int)bytes_recvd;
				REPL_DPRINT2("repl_recv: returning with recv_len %ld\n", bytes_recvd);
				/* Trace last REPL_RECV_SIZE_TRACE_SIZE sizes of what was received */
				repl_recv_size_trace[repl_recv_size_trace_pos++] = bytes_recvd;
				repl_recv_size_trace_pos %= ARRAYSIZE(repl_recv_size_trace);
				/* Trace last REPL_RECV_TRACE_BUFF_SIZE bytes received. */
				REPL_TRACE_BUFF(repl_recv_trace_buff, repl_recv_trace_buff_pos, buff, bytes_recvd,
							REPL_RECV_TRACE_BUFF_SIZE);
				return (SS_NORMAL); /* always process the received buffer before dealing with any errno */
			}
#			ifdef GTM_TLS
			if (repl_tls.enabled && ((GTMTLS_WANT_READ == bytes_recvd) || (GTMTLS_WANT_WRITE == bytes_recvd)))
			{	/* TLS/SSL implementation couldn't complete the recv() without reading or writing more data. Treat
				 * as if nothing was read. But, mark the poll direction so that then next call to repl_recv waits
				 * for the TCP/IP pipe to be ready for an I/O.
				 */
				*poll_direction = (GTMTLS_WANT_READ == bytes_recvd) ? REPL_POLLIN : REPL_POLLOUT;
				return SS_NORMAL;
			}
			/* handle error */
			save_errno = repl_tls.enabled ? gtm_tls_errno() : ERRNO;
			if (-1 == save_errno)
			{
				assert(repl_tls.enabled);
				assert(FALSE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TLSIOERROR, 2, LEN_AND_LIT("recv"), ERR_TEXT, 2,
						LEN_AND_STR(gtm_tls_get_error()));
			}
#			else
			save_errno = ERRNO;
#			endif
			if (0 == bytes_recvd)
				save_errno = ECONNRESET;
			if (EINTR == save_errno)
				continue;
			else if (ETIMEDOUT == save_errno)
			{
				repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: network may be down;"
								" socket recv() returned ETIMEDOUT\n");		/* BYPASSOK(recv) */
			} else if (EWOULDBLOCK == save_errno)
			{	/* NOTE: Although we use blocking sockets, it is possible to get EWOULDBLOCK error status if
				 * receive timeout has been set and the timeout expired before data was received (from man recv on
				 * RH 8 Linux). Some systems return ETIMEDOUT for the timeout condition.
				 */
				assert(EWOULDBLOCK != save_errno);
				repl_log(stderr, TRUE, TRUE, "Communication subsystem warning: network I/O failed to complete; "
								" Socket recv() returned EWOULDBLOCK\n");	/* BYPASSOK(recv) */
				save_errno = errno = ETIMEDOUT; /* will be treated as a bad connection and the connection closed */
			}
			break;
		}
		repl_errno = EREPL_RECV;
		return save_errno;
	} else if (!io_ready)
		return SS_NORMAL;
	save_errno = ERRNO;
	repl_errno = EREPL_SELECT;
	return save_errno;
}

int repl_close(int *sock_fd)
{
	int		status = 0;
	boolean_t	close_tls;

	/* Before closing the underlying transport, close the TLS/SSL connection */
#	ifdef GTM_TLS
	close_tls = (NULL != repl_tls.sock) && (NULL != repl_tls.sock->ssl);
	if (close_tls)
	{
		assert(REPL_TLS_REQUESTED);
		gtm_tls_socket_close(repl_tls.sock);
		assert(NULL == repl_tls.sock->ssl);
	}
	CLEAR_REPL_TLS_ENABLED;
	repl_tls.renegotiate_state = REPLTLS_RENEG_STATE_NONE;
#	endif
	if (FD_INVALID != *sock_fd)
		CLOSEFILE_RESET(*sock_fd, status);	/* resets "*sock_fd" to FD_INVALID */
	return (0 == status ? 0 : ERRNO);
}

static int get_sock_buff_size(int sockfd, int *buffsize, int which_buf)
{
	int			status;
	GTM_SOCKLEN_TYPE 	optlen;

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
#	ifndef sun
	size_t	optlen;
#	else
	int	optlen;
#	endif
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
	struct sockaddr_storage	local, remote;
	struct sockaddr		*local_sa_ptr, *remote_sa_ptr;
	GTM_SOCKLEN_TYPE	len;
	int			save_errno;
	char			*errptr, local_ip[SA_MAXLEN], remote_ip[SA_MAXLEN];
	char			port_buffer[NI_MAXSERV];
	char			local_port_buffer[NI_MAXSERV], remote_port_buffer[NI_MAXSERV];
	int			errcode;

	len = SIZEOF(local);
	local_sa_ptr = (struct sockaddr *)&local;
	remote_sa_ptr = (struct sockaddr *)&remote;
	if (0 == getsockname(sock_fd, local_sa_ptr, (GTM_SOCKLEN_TYPE *)&len))
	{
		/* translate internal address to numeric ip address */
		GETNAMEINFO(local_sa_ptr, len, local_ip, SA_MAXLEN, local_port_buffer, NI_MAXSERV, NI_NUMERICSERV, errcode);
		if (0 != errcode)
		{
			repl_log(log_fp, TRUE, TRUE, "Error getting local name info: %s\n", gai_strerror(errcode));
			strcpy(local_port_buffer, "*UNKNOWN*");
			strcpy(local_ip, "*UNKNOWN*");
		}
	} else
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		repl_log(log_fp, TRUE, TRUE, "Error getting local name: %s\n", errptr);
		strcpy(local_port_buffer, "*UNKNOWN*");
		strcpy(local_ip, "*UNKNOWN*");
	}
	len = SIZEOF(remote);
	if (0 == getpeername(sock_fd, remote_sa_ptr, (GTM_SOCKLEN_TYPE *)&len))
	{
		GETNAMEINFO(remote_sa_ptr, len, remote_ip, SA_MAXLEN, remote_port_buffer, NI_MAXSERV, NI_NUMERICSERV, errcode);
		if (0 != errcode)
		{
			repl_log(log_fp, TRUE, TRUE, "Error getting remote name info: %s\n", gai_strerror(errcode));
			strcpy(remote_port_buffer, "*UNKNOWN*");
			strcpy(remote_ip, "*UNKNOWN*");
		}
	} else
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		repl_log(log_fp, TRUE, TRUE, "Error getting remote name: %s\n", errptr);
		strcpy(remote_port_buffer, "*UNKNOWN*");
		strcpy(remote_ip, "*UNKNOWN*");
	}
	repl_log(log_fp, TRUE, TRUE, "Connection information:: Local: %s:%s Remote: %s:%s\n", local_ip, local_port_buffer,
			remote_ip, remote_port_buffer);
	return;
}

#ifdef GTM_TLS
void repl_log_tls_info(FILE *logfp, gtm_tls_socket_t *socket)
{
	char			*expiry;
	struct tm		*localtm;
	gtm_tls_conn_info	conn_info;

	if (0 != gtm_tls_get_conn_info(repl_tls.sock, &conn_info))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSCONNINFO, 0, ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error()));
		return;
	}
	repl_log(logfp, FALSE, TRUE, "TLS/SSL Session:\n");
	repl_log(logfp, FALSE, TRUE, "  Protocol Version: %s\n", conn_info.protocol);
	repl_log(logfp, FALSE, TRUE, "  Session Algorithm: %s\n", conn_info.session_algo);
	repl_log(logfp, FALSE, TRUE, "  Compression: %s\n", conn_info.compression);
	repl_log(logfp, FALSE, TRUE, "  Session Reused: %s\n", conn_info.reused ? "YES" : "NO");
	if ('\0' != conn_info.session_id[0])
		repl_log(logfp, FALSE, TRUE, "  Session-ID: %s\n", conn_info.session_id);
	if (-1 == conn_info.session_expiry_timeout)
		expiry = "NEVER\n";	/* '\n' is added because asctime always appends a '\n' at the end. */
	else
	{
		GTM_LOCALTIME(localtm, (time_t *)&conn_info.session_expiry_timeout);
		expiry = asctime(localtm);	/* BYPASSOK to prevent workcheck wanting to convert *time(..) to GTM_*TIME(..)*/
	}
	repl_log(logfp, FALSE, TRUE, "  Session Expiry: %s", expiry);
	repl_log(logfp, FALSE, TRUE, "  Secure Renegotiation %s supported\n", conn_info.secure_renegotiation ? "IS" : "IS NOT");
	repl_log(logfp, FALSE, TRUE, "Peer Certificate Information:\n");
	repl_log(logfp, FALSE, TRUE, "  Asymmetric Algorithm: %s (%d bit)\n", conn_info.cert_algo, conn_info.cert_nbits);
	repl_log(logfp, FALSE, TRUE, "  Subject: %s\n", conn_info.subject);
	repl_log(logfp, FALSE, TRUE, "  Issuer: %s\n", conn_info.issuer);
	repl_log(logfp, FALSE, TRUE, "  Validity:\n");
	repl_log(logfp, FALSE, TRUE, "    Not Before: %s\n", conn_info.not_before);
	repl_log(logfp, FALSE, TRUE, "    Not After: %s\n", conn_info.not_after);
}

void repl_do_tls_init(FILE *logfp)
{
	boolean_t	issue_fallback_warning, status;

	assert(REPL_TLS_REQUESTED);
	assert(NULL == tls_ctx);
	assert(!repl_tls.enabled);
	issue_fallback_warning = FALSE;
	if (SS_NORMAL != (status = gtm_tls_loadlibrary()))
	{
		if (!PLAINTEXT_FALLBACK)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSDLLNOOPEN, 0, ERR_TEXT, 2, LEN_AND_STR(dl_err));
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_WARNING(ERR_TLSDLLNOOPEN), 0, ERR_TEXT, 2,
					LEN_AND_STR(dl_err));
		issue_fallback_warning = TRUE;
	} else if (NULL == (tls_ctx = gtm_tls_init(GTM_TLS_API_VERSION, 0)))
	{
		if (!PLAINTEXT_FALLBACK)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSINIT, 0, ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error()));
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_WARNING(ERR_TLSINIT), 0, ERR_TEXT, 2,
					LEN_AND_STR(gtm_tls_get_error()));
		issue_fallback_warning = TRUE;
	}
	if (issue_fallback_warning)
	{
		repl_log(logfp, TRUE, TRUE, "Plaintext fallback enabled. TLS/SSL communication will not be attempted again.\n");
		CLEAR_REPL_TLS_REQUESTED;	/* As if -tlsid qualifier was never specified. */
	} else
	{
		assert(NULL != tls_ctx);
		repl_log(logfp, TRUE, FALSE, "TLS/SSL library for secure communication successfully loaded. ");
		repl_log(logfp, FALSE, TRUE, "Runtime library version: 0x%08x; FIPS Mode: %s\n",
				GTMTLS_RUNTIME_LIB_VERSION(tls_ctx),  GTMTLS_IS_FIPS_MODE(tls_ctx) ? "ON" : "OFF");
	}
}

int repl_do_tls_handshake(FILE *logfp, int sock_fd, boolean_t do_accept, int *poll_direction)
{
	int			io_ready, save_errno, status;

	assert(REPL_TLS_REQUESTED);
	assert(NULL != tls_ctx);
	assert(NULL != repl_tls.sock);
	assert(!repl_tls.enabled);
	if (REPL_INVALID_POLL_DIRECTION != *poll_direction)
	{
		if (0 >= (io_ready = fd_ioready(sock_fd, *poll_direction, REPL_POLL_WAIT)))
		{
			if (!io_ready)
				return REPL_POLLIN == *poll_direction ? GTMTLS_WANT_READ : GTMTLS_WANT_WRITE;
			save_errno = errno;
			repl_errno = EREPL_SELECT;
			return save_errno;
		}
		/* Fall-through */
	}
	/* Do the TLS/SSL handshake */
	save_errno = SS_NORMAL;
	if (-1 == (status = do_accept ? gtm_tls_accept(repl_tls.sock) : gtm_tls_connect(repl_tls.sock)))
		save_errno = gtm_tls_errno();
	else if ((GTMTLS_WANT_READ == status) || (GTMTLS_WANT_WRITE == status))
	{
		*poll_direction = GTMTLS_WANT_READ ? REPL_POLLIN : REPL_POLLOUT;
		return status;
	} else
	{	/* TLS/SSL handshake succeeded. Log information about the peer's certificate and the TLS/SSL connection. */
		repl_log(logfp, TRUE, TRUE, "Secure communication enabled using TLS/SSL protocol\n");
		repl_log_tls_info(logfp, repl_tls.sock);
		return SS_NORMAL;
	}
	assert(SS_NORMAL != save_errno);
	repl_errno = do_accept ? EREPL_RECV : EREPL_SEND;
	return save_errno;
}
#endif
