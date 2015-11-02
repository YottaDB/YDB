/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_bind.c */
#include "mdef.h"
#include <errno.h>
#include "gtm_time.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include <netinet/tcp.h>
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "iotcproutine.h"

#define	BOUND	"BOUND"

GBLREF	tcp_library_struct	tcp_routines;

boolean_t iosocket_bind(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz)
{
	int		temp_1 = 1;
	char		*errptr;
	int4		errlen, msec_timeout, real_errno;
	short		len;
	in_port_t	actual_port;
	boolean_t	no_time_left = FALSE;
	d_socket_struct *dsocketptr;
	ABS_TIME        cur_time, end_time;
	GTM_SOCKLEN_TYPE	addrlen;
	GTM_SOCKLEN_TYPE	sockbuflen;

	error_def(ERR_SOCKINIT);
	error_def(ERR_GETSOCKOPTERR);
	error_def(ERR_SETSOCKOPTERR);
	error_def(ERR_GETSOCKNAMERR);

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
                        tcp_routines.aa_close(socketptr->sd);
		if (-1 == (socketptr->sd = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, 0)))
		{
		        real_errno = errno;
			errptr = (char *)STRERROR(real_errno);
         		errlen = STRLEN(errptr);
                	rts_error(VARLSTCNT(5) ERR_SOCKINIT, 3, real_errno, errlen, errptr);
			return FALSE;
		}
		temp_1 = 1;
		if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
				SOL_SOCKET, SO_REUSEADDR, &temp_1, SIZEOF(temp_1)))
		{
		        real_errno = errno;
			errptr = (char *)STRERROR(real_errno);
         		errlen = STRLEN(errptr);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_REUSEADDR"), real_errno, errlen, errptr);
			return FALSE;
		}
#ifdef TCP_NODELAY
		temp_1 = socketptr->nodelay ? 1 : 0;
		if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
				IPPROTO_TCP, TCP_NODELAY, &temp_1, SIZEOF(temp_1)))
		{
		        real_errno = errno;
			errptr = (char *)STRERROR(real_errno);
         		errlen = STRLEN(errptr);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("TCP_NODELAY"), real_errno, errlen, errptr);
			return FALSE;
		}
#endif
		if (update_bufsiz)
		{
			if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
				SOL_SOCKET, SO_RCVBUF, &socketptr->bufsiz, SIZEOF(socketptr->bufsiz)))
			{
			        real_errno = errno;
				errptr = (char *)STRERROR(real_errno);
         			errlen = STRLEN(errptr);
                		rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_RCVBUF"), real_errno, errlen, errptr);
				return FALSE;
			}
		} else
		{
			sockbuflen = SIZEOF(socketptr->bufsiz);
			if (-1 == tcp_routines.aa_getsockopt(socketptr->sd,
				SOL_SOCKET, SO_RCVBUF, &socketptr->bufsiz, &sockbuflen))
			{
			        real_errno = errno;
				errptr = (char *)STRERROR(real_errno);
         			errlen = STRLEN(errptr);
                		rts_error(VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_RCVBUF"), real_errno, errlen, errptr);
				return FALSE;
			}
		}
		temp_1 = tcp_routines.aa_bind(socketptr->sd,
				(struct sockaddr *)&socketptr->local.sin, SIZEOF(struct sockaddr));
		if (temp_1 < 0)
		{
			real_errno = errno;
                        no_time_left = TRUE;
                        switch (real_errno)
                        {
				case EADDRINUSE:
					if (NO_M_TIMEOUT != timepar)
					{
						sys_get_curr_time(&cur_time);
						cur_time = sub_abs_time(&end_time, &cur_time);
						if (cur_time.at_sec > 0)
							no_time_left = FALSE;
					}
					break;
				case EINTR:
					break;
				default:
					errptr = (char *)STRERROR(real_errno);
					errlen = STRLEN(errptr);
					rts_error(VARLSTCNT(5) ERR_SOCKINIT, 3, real_errno, errlen, errptr);
					break;
                        }
                        if (no_time_left)
                                return FALSE;
                        hiber_start(100);
                }
	} while (temp_1 < 0);

	addrlen = SIZEOF(socketptr->local.sin);
	if (-1 == tcp_routines.aa_getsockname(socketptr->sd, (struct sockaddr *)&socketptr->local.sin, &addrlen))
	{
		real_errno = errno;
		errptr = (char *)STRERROR(real_errno);
		errlen = STRLEN(errptr);
		rts_error(VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, real_errno, errlen, errptr);
	        return FALSE;
	}
        actual_port = GTM_NTOHS(socketptr->local.sin.sin_port);
        if (0 == socketptr->local.port)
        	socketptr->local.port = actual_port;
       	assert(socketptr->local.port == actual_port);
	socketptr->state = socket_bound;
	len = SIZEOF(BOUND) - 1;
        memcpy(&dsocketptr->dollar_key[0], BOUND, len);
        dsocketptr->dollar_key[len++] = '|';
        memcpy(&dsocketptr->dollar_key[len], socketptr->handle, socketptr->handle_len);
        len += socketptr->handle_len;
        dsocketptr->dollar_key[len++] = '|';
        SPRINTF(&dsocketptr->dollar_key[len], "%d", socketptr->local.port);
	return TRUE;
}
