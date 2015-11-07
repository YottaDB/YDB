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

/*
 * tcp_open.
 *
 *      parameters:
 *		     host - name like beowulf.sanchez.com or NULL
 *		     port - numeric port number
 *		     timeout - numeric seconds
 *		     passive - boolean 0 is sender and must have a host, 1 may
 *		               have a host and if so, it is checked to see that
 *		               incomming connections are from that host.
 *      return:
 *                  socket descriptor for the connection or
 *                  -1 and output the error.
 *
 * Note that on error return errno may not be at the same value as the error condition.
 *
 */

#include "mdef.h"

#include <errno.h>
#include "gtm_time.h"
#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_inet.h"
#include "gtm_string.h"
#include "gtm_ctype.h"
#include "gtm_stdio.h"

#include "copy.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcp_select.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "io_params.h"
#include "util.h"
#include "gtmmsg.h"

#define	MAX_CONN_PENDING	5
#define	NAP_LENGTH		300

#ifdef __osf__
/* Tru64 does not have the prototype for "hstrerror" even though the function is available in the library.
 * Until we revamp the TCP communications setup stuff to use the new(er) POSIX definitions, we cannot move
 * away from "hstrerror". Declare prototype for this function in Tru64 manually until then.
 */
const char *hstrerror(int err);
#endif

GBLREF tcp_library_struct       tcp_routines;

error_def(ERR_GETADDRINFO);
error_def(ERR_GETNAMEINFO);
error_def(ERR_INVADDRSPEC);
error_def(ERR_IPADDRREQ);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKACPT);
error_def(ERR_SOCKINIT);
error_def(ERR_SOCKLISTEN);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

int tcp_open(char *host, unsigned short port, int4 timeout, boolean_t passive) /* host needs to be NULL terminated */
{
	boolean_t		no_time_left = FALSE, error_given = FALSE;
	char			temp_addr[SA_MAXLEN + 1], addr[SA_MAXLEN + 1];
	char 			*from, *to, *errptr, *temp_ch;
	char			ipname[SA_MAXLEN];
	int			match, sock, sendbufsize, ii, on = 1, temp_1 = -2;
	GTM_SOCKLEN_TYPE	size;
	int4                    rv, msec_timeout;
	struct addrinfo		*ai_ptr = NULL, *remote_ai_ptr = NULL, *remote_ai_head, hints;
	char			port_buffer[NI_MAXSERV], *brack_pos;

	int			host_len, addr_len, port_len;
	char                    msg_buffer[1024];
	mstr                    msg_string;
	ABS_TIME                cur_time, end_time;
	fd_set                  tcp_fd;
	struct sockaddr_storage peer;
	short 			retry_num;
	int 			save_errno, errlen;
	const char		*terrptr;
	int			errcode;
	boolean_t		af;

	msg_string.len = SIZEOF(msg_buffer);
	msg_string.addr = msg_buffer;
	/* ============================= initialize structures ============================== */
	if (NULL != host)
	{
		host_len = strlen(host);
		if ('[' == host[0])
		{
			brack_pos = memchr(host, ']', SA_MAXLEN);
			if (NULL == brack_pos || (&host[1] == brack_pos))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVADDRSPEC);
				return -1;
			}
			addr_len = brack_pos - &(host[1]);
			memcpy(addr, &host[1], addr_len);
			if ('\0' != *(brack_pos + 1))
			{	/* not allowed to have special symbols other than [ and ] */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVADDRSPEC);
				return -1;
			}
		} else
		{	/* IPv4 address only */
			addr_len = strlen(host);
			if (0 == addr_len)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVADDRSPEC);
				return -1;
			}
			memcpy(addr, &host[0], addr_len);
		}
		addr[addr_len] = '\0';
		CLIENT_HINTS(hints);
		port_len = 0;
		I2A(port_buffer, port_len, port);
		port_buffer[port_len]='\0';
		if (0  != (errcode = getaddrinfo(addr, port_buffer, &hints, &remote_ai_head)))
		{
			RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
			return -1;
		}
	}

	/* ============================== do the connection ============================== */
	if (passive)
	{
		struct timeval  utimeout, save_utimeout;
		int 		lsock;

		af = ((GTM_IPV6_SUPPORTED && !ipv4_only) ? AF_INET6 : AF_INET);
		lsock = tcp_routines.aa_socket(af, SOCK_STREAM, IPPROTO_TCP);
		if (-1 == lsock)
		{
			af = AF_INET;
			if (-1 == (lsock = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
        	 		errlen = STRLEN(errptr);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, save_errno, errlen, errptr);
				assert(FALSE);
				return -1;
			}
		}
		SERVER_HINTS(hints, af);
		/* We can only listen on our own system */
		port_len = 0;
		I2A(port_buffer, port_len, port);
		port_buffer[port_len]='\0';
		if (0 != (errcode = getaddrinfo(NULL, port_buffer, &hints, &ai_ptr)))
		{
			RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
			return -1;

		}
		/* allow multiple connections to the same IP address */
		if (-1 == tcp_routines.aa_setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)))
		{
			save_errno = errno;
			(void)tcp_routines.aa_close(lsock);
			errptr = (char *)STRERROR(save_errno);
         		errlen = STRLEN(errptr);
                	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					LEN_AND_LIT("SO_REUSEADDR"), save_errno, errlen, errptr);
			assert(FALSE);
			return -1;
		}
		if (-1 == tcp_routines.aa_bind(lsock, ai_ptr->ai_addr, ai_ptr->ai_addrlen))
		{
			save_errno = errno;
			(void)tcp_routines.aa_close(lsock);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				   LEN_AND_LIT("bind()"), CALLFROM, save_errno);
			return -1;
		}
		freeaddrinfo(ai_ptr);
		/* establish a queue of length MAX_CONN_PENDING for incoming connections */
		if (-1 == tcp_routines.aa_listen(lsock, MAX_CONN_PENDING))
		{
			save_errno = errno;
			(void)tcp_routines.aa_close(lsock);
			errptr = (char *)STRERROR(save_errno);
			errlen = STRLEN(errptr);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKLISTEN, 0, ERR_TEXT, 2, errlen, errptr);
			assert(FALSE);
			return -1;
		}
		if (NO_M_TIMEOUT != timeout)
		{
			msec_timeout = timeout2msec(timeout);
			sys_get_curr_time(&cur_time);
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			utimeout.tv_sec = timeout;
			utimeout.tv_usec = 0;
		}
		FD_ZERO(&tcp_fd);
		while (TRUE)
		{
			while (TRUE)
			{
				/* The check for EINTR below is valid and should not be converted to an EINTR wrapper macro
				 * since it might be a timeout.
				 */
				FD_SET(lsock, &tcp_fd);
                                save_utimeout = utimeout;
				rv = tcp_routines.aa_select(lsock + 1, (void *)&tcp_fd, (void *)0, (void *)0,
					(NO_M_TIMEOUT == timeout) ? (struct timeval *)0 : &utimeout);
				save_errno = errno;
                                utimeout = save_utimeout;
				if ((0 <= rv) || (EINTR != save_errno))
					break;
				if (NO_M_TIMEOUT != timeout)
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					if (0 >= (utimeout.tv_sec = cur_time.at_sec))
					{
						rv = 0;
						break;
					}
				}
			}
			if (0 == rv)
			{
				(void)tcp_routines.aa_close(lsock);
				util_out_print("Listening timed out.\n", TRUE);
				assert(FALSE);
				return -1;
			} else  if (0 > rv)
			{
				(void)tcp_routines.aa_close(lsock);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					   LEN_AND_LIT("select()"), CALLFROM, save_errno);
				assert(FALSE);
				return -1;
			}
			size = SIZEOF(struct sockaddr_storage);
			sock = tcp_routines.aa_accept(lsock, (struct sockaddr*)(&peer), &size);
			if (FD_INVALID == sock)
			{
				save_errno = errno;
#				ifdef __hpux
				if (ENOBUFS == save_errno)
					continue;
#				endif
				(void)tcp_routines.aa_close(lsock);
				errptr = (char *)STRERROR(save_errno);
				errlen = STRLEN(errptr);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKACPT, 0, ERR_TEXT, 2, errlen, errptr);
				assert(FALSE);
				return -1;
			}
			GETNAMEINFO((struct sockaddr*)(&peer), size, temp_addr, SA_MAXLEN + 1, NULL, 0, NI_NUMERICHOST, errcode);
			if (0 != errcode)
			{
				RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
				return -1;
			}
#			ifdef	DEBUG_ONLINE
			PRINTF("Connection is from : %s\n", &temp_addr[0]);
#			endif
			break;
			/* previously there is following check here
			 * if ((0 == temp_sin_addr) || (0 == memcmp(&addr[0], &temp_addr[0], strlen(addr))))
                         * However, temp_sin_addr is always 0 on server side, and addr(local address) shoud not equal to
			 * temp_addr(remote address), so the entire check should be removed
			 */
		}
		(void)tcp_routines.aa_close(lsock);
	} else
	{	/* client side (connection side) */
		if (NO_M_TIMEOUT != timeout)
		{
			msec_timeout = timeout2msec(timeout);
			sys_get_curr_time(&cur_time);
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
		}
		no_time_left = FALSE;
		temp_1 = 1;
		do
		{
			if (1 != temp_1)
				tcp_routines.aa_close(sock);
			assert(NULL != remote_ai_head);
			for (remote_ai_ptr = remote_ai_head; NULL != remote_ai_ptr; remote_ai_ptr = remote_ai_ptr->ai_next)
			{
				sock = tcp_routines.aa_socket(remote_ai_ptr->ai_family, remote_ai_ptr->ai_socktype,
							      remote_ai_ptr->ai_protocol);
				if (FD_INVALID != sock)
					break;
			}
			if (FD_INVALID == sock)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				errlen = STRLEN(errptr);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, save_errno, errlen, errptr);
				assert(FALSE);
				return -1;
			}
			/* Allow multiple connections to the same IP address */
			if (-1 == tcp_routines.aa_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)))
			{
				save_errno = errno;
				(void)tcp_routines.aa_close(sock);
				errptr = (char *)STRERROR(save_errno);
				errlen = STRLEN(errptr);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					   LEN_AND_LIT("SO_REUSEADDR"), save_errno, errlen, errptr);
				assert(FALSE);
				return -1;
			}
			temp_1 = tcp_routines.aa_connect(sock, remote_ai_ptr->ai_addr, remote_ai_ptr->ai_addrlen);
			save_errno = errno;
			/* tcp_routines.aa_connect == gtm_connect should have handled EINTR. Assert that */
			assert((0 <= temp_1) || (EINTR != save_errno));
			if ((0 > temp_1) && (ECONNREFUSED != save_errno))
			{
				(void)tcp_routines.aa_close(sock);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					   LEN_AND_LIT("connect()"), CALLFROM, save_errno);
				assert(FALSE);
				return -1;
			}
			if ((temp_1 < 0) && (NO_M_TIMEOUT != timeout))
			{
				sys_get_curr_time(&cur_time);
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (cur_time.at_sec <= 0)
					no_time_left = TRUE;
			}
			SHORT_SLEEP(NAP_LENGTH);               /* Sleep for NAP_LENGTH ms */
		} while ((FALSE == no_time_left) && (0 > temp_1));

		freeaddrinfo(remote_ai_head);
		if (0 > temp_1) /* out of time */
		{
			tcp_routines.aa_close(sock);
			util_out_print("Connection timed out.", TRUE);
			assert(FALSE);
			return -1;
		}
	}
	return sock;
}
