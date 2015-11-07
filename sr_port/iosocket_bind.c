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

/* iosocket_bind.c */
#include "mdef.h"
#include <errno.h>
#include "gtm_time.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "iotcproutine.h"
#include "gtm_stdlib.h"

#define	BOUND	"BOUND"
#define IPV6_UNCERTAIN 2

GBLREF	tcp_library_struct	tcp_routines;

error_def(ERR_GETNAMEINFO);
error_def(ERR_GETSOCKNAMERR);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKBIND);
error_def(ERR_SOCKINIT);
error_def(ERR_TEXT);

boolean_t iosocket_bind(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz)
{
	int			temp_1 = 1;
	char			*errptr;
	int4			errlen, msec_timeout, real_errno;
	short			len;
	in_port_t		actual_port;
	boolean_t		no_time_left = FALSE;
	d_socket_struct		*dsocketptr;
	struct addrinfo		*ai_ptr;
	char			port_buffer[NI_MAXSERV];
	int			errcode;
	ABS_TIME        	cur_time, end_time;
	GTM_SOCKLEN_TYPE	addrlen;
	GTM_SOCKLEN_TYPE	sockbuflen;

	dsocketptr = socketptr->dev;
	ai_ptr = (struct addrinfo*)(&socketptr->local.ai);
	assert(NULL != dsocketptr);
	dsocketptr->iod->dollar.key[0] = '\0';
	if (FD_INVALID != socketptr->temp_sd)
	{
		socketptr->sd = socketptr->temp_sd;
		socketptr->temp_sd = FD_INVALID;
	}
	if (timepar != NO_M_TIMEOUT)
	{
		msec_timeout = timeout2msec(timepar);
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
	}

	do
	{
		temp_1 = 1;
		if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
				SOL_SOCKET, SO_REUSEADDR, &temp_1, SIZEOF(temp_1)))
		{
		        real_errno = errno;
			errptr = (char *)STRERROR(real_errno);
			errlen = STRLEN(errptr);
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
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
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
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
				SOCKET_FREE(socketptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
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
				SOCKET_FREE(socketptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_RCVBUF"), real_errno, errlen, errptr);
				return FALSE;
			}
		}
		temp_1 = tcp_routines.aa_bind(socketptr->sd, SOCKET_LOCAL_ADDR(socketptr), ai_ptr->ai_addrlen);
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
					SOCKET_FREE(socketptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBIND, 0, real_errno, 0);
					break;
			}
			if (no_time_left)
				return FALSE;
			hiber_start(100);
			tcp_routines.aa_close(socketptr->sd);
			if (-1 == (socketptr->sd = tcp_routines.aa_socket(ai_ptr->ai_family,ai_ptr->ai_socktype,
									  ai_ptr->ai_protocol)))
			{
				real_errno = errno;
				errptr = (char *)STRERROR(real_errno);
				errlen = STRLEN(errptr);
				SOCKET_FREE(socketptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, real_errno, errlen, errptr);
				return FALSE;
			}
		}
	} while (temp_1 < 0);

	/* obtain actual port from the bound address if port 0 was specified */
	addrlen = SOCKET_ADDRLEN(socketptr, ai_ptr, local);
	if (-1 == tcp_routines.aa_getsockname(socketptr->sd, SOCKET_LOCAL_ADDR(socketptr), &addrlen))
	{
		real_errno = errno;
		errptr = (char *)STRERROR(real_errno);
		errlen = STRLEN(errptr);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, real_errno, errlen, errptr);
	        return FALSE;
	}
	assert(ai_ptr->ai_addrlen == addrlen);
	GETNAMEINFO(SOCKET_LOCAL_ADDR(socketptr), addrlen, NULL, 0, port_buffer, NI_MAXSERV, NI_NUMERICSERV, errcode);
	if (0 != errcode)
	{
		SOCKET_FREE(socketptr);
		RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
		return FALSE;
	}
	actual_port = ATOI(port_buffer);
	if (0 == socketptr->local.port)
		socketptr->local.port = actual_port;
	assert(socketptr->local.port == actual_port);
	socketptr->state = socket_bound;
	len = SIZEOF(BOUND) - 1;
        memcpy(&dsocketptr->iod->dollar.key[0], BOUND, len);
        dsocketptr->iod->dollar.key[len++] = '|';
        memcpy(&dsocketptr->iod->dollar.key[len], socketptr->handle, socketptr->handle_len);
        len += socketptr->handle_len;
        dsocketptr->iod->dollar.key[len++] = '|';
        SPRINTF(&dsocketptr->iod->dollar.key[len], "%d", socketptr->local.port);
	return TRUE;
}
