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

/* iosocket_connect.c */
#include "mdef.h"
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef __MVS__
#include <netinet/tcp.h>
#endif
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "iotcproutine.h"

#define	ESTABLISHED	"ESTABLISHED"
GBLREF	tcp_library_struct	tcp_routines;
boolean_t iosocket_connect(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz)
{
	int		temp_1 = 1;
	char		*errptr;
	int4            errlen, msec_timeout, real_errno;
	boolean_t	no_time_left = FALSE;
	short		len;
	d_socket_struct *dsocketptr;
	ABS_TIME        cur_time, end_time;

	error_def(ERR_SOCKINIT);
	error_def(ERR_OPENCONN);
	error_def(ERR_TEXT);
	error_def(ERR_GETSOCKOPTERR);
	error_def(ERR_SETSOCKOPTERR);
	dsocketptr = socketptr->dev;
        assert(NULL != dsocketptr);
        dsocketptr->dollar_key[0] = '\0';
	if (timepar != NO_M_TIMEOUT)
	{
		msec_timeout = timeout2msec(timepar);
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
	}
	do
	{
		if (1 != temp_1)
		{
			tcp_routines.aa_close(socketptr->sd);
		}
	        if (-1 == (socketptr->sd = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, 0)))
        	{
                	errptr = (char *)STRERROR(errno);
                	errlen = strlen(errptr);
                	rts_error(VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
                	return FALSE;
        	}
		temp_1 = 1;
		if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
				SOL_SOCKET, SO_REUSEADDR, &temp_1, sizeof(temp_1)))
        	{
                	errptr = (char *)STRERROR(errno);
                	errlen = strlen(errptr);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_REUSEADDR"), errno, errlen, errptr);
                	return FALSE;
        	}
		temp_1 = socketptr->nodelay ? 1 : 0;
		if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
				IPPROTO_TCP, TCP_NODELAY, &temp_1, sizeof(temp_1)))
        	{
                	errptr = (char *)STRERROR(errno);
                	errlen = strlen(errptr);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("TCP_NODELAY"), errno, errlen, errptr);
                	return FALSE;
        	}
		if (update_bufsiz)
		{
			if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
				SOL_SOCKET, SO_RCVBUF, &socketptr->bufsiz, sizeof(socketptr->bufsiz)))
			{
				errptr = (char *)STRERROR(errno);
         			errlen = strlen(errptr);
                		rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_RCVBUF"), errno, errlen, errptr);
				return FALSE;
			}
		}
		else
		{
			temp_1 = sizeof(socketptr->bufsiz);
			if (-1 == tcp_routines.aa_getsockopt(socketptr->sd,
				SOL_SOCKET, SO_RCVBUF, &socketptr->bufsiz, &temp_1))
			{
				errptr = (char *)STRERROR(errno);
         			errlen = strlen(errptr);
                		rts_error(VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_RCVBUF"), errno, errlen, errptr);
				return FALSE;
			}
		}
		temp_1 = tcp_routines.aa_connect(socketptr->sd,
				(struct sockaddr *)&socketptr->remote.sin, sizeof(socketptr->remote.sin));
		if (temp_1 < 0)
		{
			real_errno = errno;
			no_time_left = TRUE;
			switch (real_errno)
			{
			case ETIMEDOUT	:	/* the other side bound but not listening */
			case ECONNREFUSED :
				if (NO_M_TIMEOUT != timepar)
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					if (cur_time.at_sec > 0)
						no_time_left = FALSE;
				}
				break;
			case EINTR :
				break;
			default:
				errptr = (char *)STRERROR(real_errno);
				errlen = strlen(errptr);
				rts_error(VARLSTCNT(6) ERR_OPENCONN, 0, ERR_TEXT, 2, errlen, errptr);
				break;
			}
			if (no_time_left)
			{
				return FALSE;
			}
			hiber_start(100);
		}
	} while (temp_1 < 0);
	/* handle the local information later.
	SPRINTF(socketptr->local.saddr_ip, "%s", tcp_routines.aa_inet_ntoa(socketptr->remote.sin.sin_addr));
	socketptr->local.port = GTM_NTOHS(socketptr->remote.sin.sin_port);
	*/
	socketptr->state = socket_connected;
	/* update dollar_key */
        len = sizeof(ESTABLISHED) - 1;
        memcpy(&dsocketptr->dollar_key[0], ESTABLISHED, len);
        dsocketptr->dollar_key[len++] = '|';
        memcpy(&dsocketptr->dollar_key[len], socketptr->handle, socketptr->handle_len);
        len += socketptr->handle_len;
        dsocketptr->dollar_key[len++] = '|';
	memcpy(&dsocketptr->dollar_key[len], socketptr->remote.saddr_ip, strlen(socketptr->remote.saddr_ip));
	len += strlen(socketptr->remote.saddr_ip);
	dsocketptr->dollar_key[len] = '\0';
	return TRUE;
}
