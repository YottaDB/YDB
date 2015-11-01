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

/*
 * tcp_open
 *		*** iotcp_open.c can be recoded to use this module ***
 *
 *      parameter -- a string like beowulf.sanchez.com:6100:10
 *                   which means to open a tcp connection to
 *                   beowulf.sanchez.com at port 6100 with a
 *                   10 seconds timeout.
 *      return    -- socket descriptor for the connection or
 *                   -1  and output the error.
 */

#include "mdef.h"

#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef UNIX
#include "gtm_inet.h"
#endif
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
#define	NAP_LENGTH		100

GBLREF tcp_library_struct       tcp_routines;

int tcp_open(char *host, unsigned short port, int4 timeout, boolean_t passive) /* host needs to be NULL terminated */
{
	boolean_t		no_time_left = FALSE;
	char			temp_addr[SA_MAXLEN + 1], addr[SA_MAXLEN + 1];
	char 			*from, *to, *errptr, *temp_ch;
	int			match, sock, sendbufsize, size, ii, on = 1, temp_1 = -2;
	int4                    errlen, rv, msec_timeout;
	struct	sockaddr_in	sin;
	char                    msg_buffer[1024];
	mstr                    msg_string;
	ABS_TIME                cur_time, end_time;
	fd_set                  tcp_fd;
	struct sockaddr_in      peer;

	error_def(ERR_INVADDRSPEC);

	msg_string.len = sizeof(msg_buffer);
	msg_string.addr = msg_buffer;
	memset((char *)&sin, 0, sizeof(struct sockaddr_in));

	if (NULL == host)
	{
		if (!passive)
		{
			util_out_print("An address has to be specified for an active connection.", TRUE);
			return -1;
		}
		sin.sin_addr.s_addr = INADDR_ANY;
	}
	else
	{
		temp_ch = host;
		while(ISDIGIT(*temp_ch) || ('.' == *temp_ch))
			temp_ch++;
		if ('\0' != *temp_ch)
			SPRINTF(addr, "%s", iotcp_name2ip(host));
		else
			SPRINTF(addr, "%s", host);

		if (-1 == (sin.sin_addr.s_addr = tcp_routines.aa_inet_addr(addr)))
		{
			gtm_getmsg(ERR_INVADDRSPEC, &msg_string);
			util_out_print(msg_string.addr, TRUE, ERR_INVADDRSPEC);
			return  -1;
		}
		sin.sin_port = GTM_HTONS(port);
		sin.sin_family = AF_INET;
	}


	/* ============================== do the connection ============================== */

	if (passive)
	{
		struct timeval  utimeout, save_utimeout;
		int 		lsock;

		lsock = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, 0);
		if (-1 == lsock)
		{
			errptr = (char *)STRERROR(errno);
			errlen = strlen(errptr);
			util_out_print(errptr, TRUE, errno);
			return -1;
		}
		/* allow multiple connections to the same IP address */
		if (-1 == tcp_routines.aa_setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
		{
			errptr = (char *)STRERROR(errno);
			errlen = strlen(errptr);
			(void)tcp_routines.aa_close(lsock);
			util_out_print(errptr, TRUE, errno);
			return -1;
		}
		if (-1 == tcp_routines.aa_bind(lsock, (struct sockaddr *)&sin, sizeof(struct sockaddr)))
		{
			errptr = (char *)STRERROR(errno);
			errlen = strlen(errptr);
			(void)tcp_routines.aa_close(lsock);
			util_out_print(errptr, TRUE, errno);
			return -1;
		}
		/* establish a queue of length MAX_CONN_PENDING for incoming connections */
		if (-1 == tcp_routines.aa_listen(lsock, MAX_CONN_PENDING))
		{
			errptr = (char *)STRERROR(errno);
			errlen = strlen(errptr);
			(void)tcp_routines.aa_close(lsock);
			util_out_print(errptr, TRUE, errno);
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
		do
		{
			do
			{
				/*
				 * the check for EINTR below is valid and should not be converted to an EINTR
				 * wrapper macro, since it might be a timeout.
				 */
				FD_ZERO(&tcp_fd);
				FD_SET(lsock, &tcp_fd);
                                save_utimeout = utimeout;
				rv = tcp_routines.aa_select(lsock + 1, (void *)&tcp_fd, (void *)0, (void *)0,
					(NO_M_TIMEOUT == timeout) ? (struct timeval *)0 : &utimeout);
                                utimeout = save_utimeout;
				if ((rv >= 0) || (EINTR != errno))
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
			} while (1);
			if (0 == rv)
			{
				util_out_print("Listening timed out.\n", TRUE);
				(void)tcp_routines.aa_close(lsock);
				return -1;
			}
			else  if (rv < 0)
			{
				errptr = (char *)STRERROR(errno);
				errlen = strlen(errptr);
				util_out_print(errptr, TRUE, errno);
				(void)tcp_routines.aa_close(lsock);
				return -1;
			}
			size = sizeof(struct sockaddr_in);
			sock = tcp_routines.aa_accept(lsock, &peer, &size);
			if (-1 == sock)
			{
				errptr = (char *)STRERROR(errno);
				errlen = strlen(errptr);
				util_out_print(errptr, TRUE, errno);
				(void)tcp_routines.aa_close(lsock);
				return -1;
			}
			SPRINTF(&temp_addr[0], "%s", tcp_routines.aa_inet_ntoa(peer.sin_addr));
#ifdef	DEBUG_ONLINE
			PRINTF("Connection is from : %s\n", &temp_addr[0]);
#endif
			if ((INADDR_ANY == sin.sin_addr.s_addr) || (0 == memcmp(&addr[0], &temp_addr[0], strlen(addr))))
				break;
			else
			{
				(void)tcp_routines.aa_close(sock);
				if (NO_M_TIMEOUT != timeout)
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					utimeout.tv_sec = ((cur_time.at_sec > 0) ? cur_time.at_sec : 0);
				}
			}
		} while  (1);
		(void)tcp_routines.aa_close(lsock);
	}
	else
	{
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
			if(1 != temp_1)
				tcp_routines.aa_close(sock);
			sock = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, 0);
			if (-1 == sock)
			{
				errptr = (char *)STRERROR(errno);
				errlen = strlen(errptr);
				util_out_print(errptr, TRUE, errno);
				return -1;
			}
			/*      allow multiple connections to the same IP address */
			if      (-1 == tcp_routines.aa_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
			{
				(void)tcp_routines.aa_close(sock);
				errptr = (char *)STRERROR(errno);
				errlen = strlen(errptr);
			        util_out_print(errptr, TRUE, errno);
				return -1;
			}
			temp_1 = tcp_routines.aa_connect(sock, (struct sockaddr *)(&sin), sizeof(sin));
			/*
			 * the check for EINTR below is valid and should not be converted to an EINTR
			 * wrapper macro, because other error conditions are checked, and a retry is not
			 * immediately performed.
			 */
			if ((temp_1 < 0) && (ECONNREFUSED != errno) && (EINTR != errno))
			{
				(void)tcp_routines.aa_close(sock);
				errptr = (char *)STRERROR(errno);
				errlen = strlen(errptr);
			        util_out_print(errptr, TRUE, errno);
				return -1;
			}
			if ((temp_1 < 0) && (EINTR == errno))
			{
				(void)tcp_routines.aa_close(sock);
				util_out_print("Interrupted.", TRUE);
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
		} while ((TRUE != no_time_left) && (temp_1 < 0));

		if (temp_1 < 0) /* out of time */
		{
			tcp_routines.aa_close(sock);
			util_out_print("Connection timed out.", TRUE);
			return -1;
		}
	}

	return sock;
}
