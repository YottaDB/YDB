/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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

/*
 * tcp_open.
 *
 *      parameters:
 *		     host - name like beowulf.sanchez.com or NULL
 *		     port - numeric port number
 *		     timeout - numeric seconds
 *		     passive - boolean 0 is sender and must have a host, 1 may
 *		               have a host and if so, it is ignored.
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
#include "eintr_wrappers.h"
#include "gtm_poll.h"
#include "dogetaddrinfo.h"

#include "copy.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "io_params.h"
#include "util.h"
#include "gtmmsg.h"
#include "gtmio.h"

#define	MAX_CONN_PENDING	5
#define	NAP_LENGTH		300

error_def(ERR_GETADDRINFO);
error_def(ERR_GETNAMEINFO);
error_def(ERR_INVADDRSPEC);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKACPT);
error_def(ERR_SOCKINIT);
error_def(ERR_SOCKLISTEN);
error_def(ERR_SYSCALL);
error_def(ERR_TCPCONNTIMEOUT);
error_def(ERR_TEXT);

int tcp_open(char *host, unsigned short port, uint8 timeout, boolean_t passive) /* host needs to be NULL terminated */
{
	boolean_t		no_time_left = FALSE, error_given = FALSE;
	char			temp_addr[SA_MAXLEN + 1], addr[SA_MAXLEN + 1];
	char 			*from, *to, *errptr, *temp_ch;
	char			ipname[SA_MAXLEN];
	int			match, sock = FD_INVALID, sendbufsize, ii, on = 1, temp_1 = -2;
	GTM_SOCKLEN_TYPE	size;
	int4			rv, msec_timeout;
	struct addrinfo		*ai_ptr = NULL, *remote_ai_ptr, *remote_ai_head, hints;
	char			port_buffer[NI_MAXSERV], *brack_pos;

	int			host_len, addr_len, port_len;
	ABS_TIME		cur_time, end_time;
	struct sockaddr_storage peer;
	short 			retry_num;
	int 			save_errno, rc, errlen;
	const char		*terrptr;
	int			errcode;
	boolean_t		af;
	int			poll_timeout;
	nfds_t			poll_nfds;
	struct pollfd		poll_fdlist[1];
	struct timeval  	utimeout, save_utimeout;
	int 			lsock;

	/* ============================== do the connection ============================== */
	if (passive)
	{	/* Value for host ignored as this connection is always local */

		af = ((GTM_IPV6_SUPPORTED && !ipv4_only) ? AF_INET6 : AF_INET);
		lsock = socket(af, SOCK_STREAM, IPPROTO_TCP);
		if (-1 == lsock)
		{
			af = AF_INET;
			if (-1 == (lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
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
		if (0 != (errcode = dogetaddrinfo(NULL, port_buffer, &hints, &ai_ptr)))
		{
			RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
			return -1;

		}
		/* allow multiple connections to the same IP address */
		if (-1 == setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)))
		{
			save_errno = errno;
			CLOSEFILE(lsock, rc);
			errptr = (char *)STRERROR(save_errno);
         		errlen = STRLEN(errptr);
                	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
				       LEN_AND_LIT("SO_REUSEADDR"), save_errno, errlen, errptr);
			FREEADDRINFO(ai_ptr);
			assert(FALSE);
			return -1;
		}
		if (-1 == bind(lsock, ai_ptr->ai_addr, ai_ptr->ai_addrlen))
		{
			save_errno = errno;
			CLOSEFILE(lsock, rc);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				       LEN_AND_LIT("bind()"), CALLFROM, save_errno);
			FREEADDRINFO(ai_ptr);
			return -1;
		}
		FREEADDRINFO(ai_ptr);
		/* establish a queue of length MAX_CONN_PENDING for incoming connections */
		if (-1 == listen(lsock, MAX_CONN_PENDING))
		{
			save_errno = errno;
			CLOSEFILE(lsock, rc);
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
		} else
		{	/* Initialize "utimeout" even though not necessary for correctness. This is to silence
			 * [-Wsometimes-uninitialized] warning from CLang/LLVM when "save_utimeout = utimeout" occurs below..
			 */
			utimeout.tv_sec = 0;
			utimeout.tv_usec = 0;
		}
		while (TRUE)
		{
			while (TRUE)
			{
				/* The check for EINTR below is valid and should not be converted to an EINTR wrapper macro
				 * since it might be a timeout.
				 */
				save_utimeout = utimeout;
				poll_fdlist[0].fd = lsock;
				poll_fdlist[0].events = POLLIN;
				poll_nfds = 1;
				if (NO_M_TIMEOUT == timeout)
					poll_timeout = -1;
				else
					poll_timeout = (long)((utimeout.tv_sec * MILLISECS_IN_SEC) +
						DIVIDE_ROUND_UP(utimeout.tv_usec, MICROSECS_IN_MSEC));
				rv = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
				save_errno = errno;
                                utimeout = save_utimeout;
				if ((0 <= rv) || (EINTR != save_errno))
					break;
				eintr_handling_check();
				if (NO_M_TIMEOUT != timeout)
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					if (0 >= (utimeout.tv_sec = cur_time.tv_sec))
					{
						rv = 0;
						break;
					}
				}
			}
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			if (0 == rv)
			{	/* Timeout */
				CLOSEFILE(lsock, rc);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_TCPCONNTIMEOUT, 1, timeout);
				return -1;
			} else  if (0 > rv)
			{
				CLOSEFILE(lsock, rc);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					   LEN_AND_LIT("poll()"), CALLFROM, save_errno);
				assert(FALSE);
				return -1;
			}
			size = SIZEOF(struct sockaddr_storage);
			ACCEPT_SOCKET(lsock, (struct sockaddr*)(&peer), &size, sock);
			if (FD_INVALID == sock)
			{
				save_errno = errno;
				CLOSEFILE(lsock, rc);
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
		}
		CLOSEFILE(lsock, rc);
	} else
	{	/* client side (connection side) */
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
		if (0  != (errcode = dogetaddrinfo(addr, port_buffer, &hints, &remote_ai_head)))
		{
			RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
			return -1;
		}
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
				CLOSEFILE(sock, rc);
			assert(NULL != remote_ai_head);
			for (remote_ai_ptr = remote_ai_head; NULL != remote_ai_ptr; remote_ai_ptr = remote_ai_ptr->ai_next)
			{
				sock = socket(remote_ai_ptr->ai_family, remote_ai_ptr->ai_socktype, remote_ai_ptr->ai_protocol);
				if (FD_INVALID != sock)
					break;
			}
			if (FD_INVALID == sock)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				errlen = STRLEN(errptr);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, save_errno, errlen, errptr);
				FREEADDRINFO(remote_ai_head);
				assert(FALSE);
				return -1;
			}
			/* Allow multiple connections to the same IP address */
			if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)))
			{
				save_errno = errno;
				FREEADDRINFO(remote_ai_head);
				CLOSEFILE(sock, rc);
				errptr = (char *)STRERROR(save_errno);
				errlen = STRLEN(errptr);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					   LEN_AND_LIT("SO_REUSEADDR"), save_errno, errlen, errptr);
				assert(FALSE);
				return -1;
			}
			CONNECT_SOCKET(sock, remote_ai_ptr->ai_addr, remote_ai_ptr->ai_addrlen, temp_1);
			save_errno = errno;
			/* CONNECT_SOCKET should have handled EINTR. Assert that */
			assert((0 <= temp_1) || (EINTR != save_errno));
			if ((0 > temp_1) && (ECONNREFUSED != save_errno))
			{
				FREEADDRINFO(remote_ai_head);
				CLOSEFILE(sock, rc);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					   LEN_AND_LIT("connect()"), CALLFROM, save_errno);
				/* We are aware that EHOSTUNREACH can happen here in times of network instability but
				 * any other error here should assert fail so we catch it and the conditions it happened
				 * under in a core file when running a debug build.
				 */
				assert(EHOSTUNREACH == save_errno);
				return -1;
			}
			if ((temp_1 < 0) && (NO_M_TIMEOUT != timeout))
			{
				sys_get_curr_time(&cur_time);
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (cur_time.tv_sec <= 0)
					no_time_left = TRUE;
			}
			SHORT_SLEEP(NAP_LENGTH);               /* Sleep for NAP_LENGTH ms */
		} while ((FALSE == no_time_left) && (0 > temp_1));

		if (0 > temp_1) /* out of time */
		{
			FREEADDRINFO(remote_ai_head);
			CLOSEFILE(sock, rc);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_TCPCONNTIMEOUT, 1, timeout);
			return -1;
		}
		FREEADDRINFO(remote_ai_head);
	}
	return sock;
}
