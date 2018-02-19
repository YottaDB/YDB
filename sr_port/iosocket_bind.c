/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "iosocketdef.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "min_max.h"
#include "gtm_stat.h"
#include "eintr_wrappers.h"		/* for STAT_FILE  and CHG_OWNER */
#define	BOUND	"BOUND"
#define IPV6_UNCERTAIN 2

error_def(ERR_GETNAMEINFO);
error_def(ERR_GETSOCKNAMERR);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKBIND);
error_def(ERR_SOCKINIT);
error_def(ERR_TEXT);

boolean_t iosocket_bind(socket_struct *socketptr, int4 msec_timeout, boolean_t update_bufsiz, boolean_t newversion)
{
	int			temp_1 = 1;
	char			*errptr, *charptr;
	int4			errlen, real_errno;
	short			len;
	in_port_t		actual_port;
	boolean_t		no_time_left = FALSE, ioerror;
	d_socket_struct		*dsocketptr;
	struct addrinfo		*ai_ptr;
	char			port_buffer[NI_MAXSERV];
	int			errcode;
	ABS_TIME		cur_time, end_time;
	GTM_SOCKLEN_TYPE	addrlen;
	GTM_SOCKLEN_TYPE	sockbuflen;
	struct stat		statbuf;
	mode_t			filemode;

	dsocketptr = socketptr->dev;
	ai_ptr = (struct addrinfo*)(&socketptr->local.ai);
	assert(NULL != dsocketptr);
	dsocketptr->iod->dollar.key[0] = '\0';
	dsocketptr->iod->dollar.device[0] = '\0';
	if (dsocketptr->iod->dollar.devicebuffer)
		free(dsocketptr->iod->dollar.devicebuffer);
	dsocketptr->iod->dollar.devicebuffer = NULL;
	ioerror = socketptr->ioerror;
	if (FD_INVALID != socketptr->temp_sd)
	{
		socketptr->sd = socketptr->temp_sd;
		socketptr->temp_sd = FD_INVALID;
	}
	if (NO_M_TIMEOUT != msec_timeout)
	{
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
	}
	do
	{
		temp_1 = 1;
		if (socket_local != socketptr->protocol)
		{
			if (-1 == setsockopt(socketptr->sd,
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
#			ifdef TCP_NODELAY
			temp_1 = socketptr->nodelay ? 1 : 0;
			if (-1 == setsockopt(socketptr->sd,
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
#			endif
			if (update_bufsiz)
			{
				if (-1 == setsockopt(socketptr->sd,
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
				if (-1 == getsockopt(socketptr->sd,
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
		}
		if (socket_local == socketptr->protocol)
		{
			charptr = ((struct sockaddr_un *)(socketptr->local.sa))->sun_path;
			STAT_FILE(charptr, &statbuf, temp_1);
			if (!temp_1)
			{
				if (!S_ISSOCK(statbuf.st_mode))
				{
					temp_1 = -1;	/* bypass unlink and issue error */
					errno = ENOTSOCK;
				}
			}
			if (!temp_1)
				if (newversion)
					temp_1 = UNLINK(charptr);
			if (temp_1 && ENOENT != errno)
			{
					real_errno = errno;
					if (ioerror)
						SOCKET_FREE(socketptr);
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(dsocketptr->iod, real_errno);
					if (ioerror)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKBIND, 0, real_errno);
					return FALSE;
			}
		}
		temp_1 = bind(socketptr->sd, SOCKET_LOCAL_ADDR(socketptr), ai_ptr->ai_addrlen);
		if (temp_1 < 0)
		{
			real_errno = errno;
			no_time_left = TRUE;
			switch (real_errno)
			{
				case EINTR:
					break;
				case EADDRINUSE:
					if (NO_M_TIMEOUT != msec_timeout)
					{
						sys_get_curr_time(&cur_time);
						cur_time = sub_abs_time(&end_time, &cur_time);
						msec_timeout = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
							/* Round up in order to prevent premature timeouts */
							DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
						if (msec_timeout > 0)
							no_time_left = FALSE;
					} else
						no_time_left = FALSE;	/* retry */
					if (socket_local != socketptr->protocol)
						break;
					/* fall through for LOCAL sockets since it unlikely the file will go away */
				default:
					if (ioerror)
						SOCKET_FREE(socketptr);
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(dsocketptr->iod, real_errno);
					if (ioerror)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKBIND, 0, real_errno);
					return FALSE;
			}
			if (no_time_left)
				return FALSE;
			hiber_start(100);
			close(socketptr->sd);
			if (-1 == (socketptr->sd = socket(ai_ptr->ai_family,ai_ptr->ai_socktype,
									  ai_ptr->ai_protocol)))
			{
				real_errno = errno;
				errptr = (char *)STRERROR(real_errno);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr);
				if (ioerror)
				{
					SOCKET_FREE(socketptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, real_errno, STRLEN(errptr),
						errptr);
				}
				return FALSE;
			}
		}
	} while (temp_1 < 0);
	/* obtain actual port from the bound address if port 0 was specified */
	addrlen = SOCKET_ADDRLEN(socketptr, ai_ptr, local);
	if (-1 == getsockname(socketptr->sd, SOCKET_LOCAL_ADDR(socketptr), &addrlen))
	{
		real_errno = errno;
		errptr = (char *)STRERROR(real_errno);
		SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr);
		if (ioerror)
		{
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, real_errno, STRLEN(errptr), errptr);
		}
		return FALSE;
	}
	if (socket_local != socketptr->protocol)
	{
		assert(ai_ptr->ai_addrlen == addrlen);	/* not right for local */
		GETNAMEINFO(SOCKET_LOCAL_ADDR(socketptr), addrlen, NULL, 0, port_buffer, NI_MAXSERV, NI_NUMERICSERV, errcode);
		if (0 != errcode)
		{
			real_errno = errno;
			TEXT_ADDRINFO(errptr, errcode, real_errno);
			SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr);
			if (ioerror)
			{
				SOCKET_FREE(socketptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GETNAMEINFO, 0, ERR_TEXT, 2, STRLEN(errptr), errptr);
			}
			return FALSE;
		}
		actual_port = ATOI(port_buffer);
		if (0 == socketptr->local.port)
			socketptr->local.port = actual_port;
		assert(socketptr->local.port == actual_port);
	} else
	{
		if (socketptr->filemode_mask)
		{
			charptr = ((struct sockaddr_un *)(socketptr->local.sa))->sun_path;
			STAT_FILE(charptr, &statbuf, temp_1);
			assertpro(!temp_1);	/* we just created socket so it should be there */
			filemode = (statbuf.st_mode & ~socketptr->filemode_mask) | socketptr->filemode;
			temp_1 = CHMOD(charptr, filemode);
			if (temp_1)
			{
				real_errno = errno;
				errptr = (char *)STRERROR(real_errno);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr);
				if (ioerror)
				{
					SOCKET_FREE(socketptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_SOCKINIT, 3, real_errno, STRLEN(errptr),
						errptr, ERR_TEXT, 2, RTS_ERROR_LITERAL("setting protection"));
				}
				return FALSE;
			}
		}
		if (((gid_t)-1 != socketptr->uic.grp) || ((uid_t)-1 != socketptr->uic.mem))
		{
			CHG_OWNER(charptr, socketptr->uic.mem, socketptr->uic.grp, temp_1);
			if (temp_1)
			{
				real_errno = errno;
				errptr = (char *)STRERROR(real_errno);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr);
				if (ioerror)
				{
					SOCKET_FREE(socketptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_SOCKINIT, 3, real_errno, STRLEN(errptr),
						errptr, ERR_TEXT, 2, RTS_ERROR_LITERAL("setting ownership"));
				}
				return FALSE;
			}
		}
	}
	socketptr->state = socket_bound;
	len = SIZEOF(BOUND) - 1;
	memcpy(&dsocketptr->iod->dollar.key[0], BOUND, len);
	dsocketptr->iod->dollar.key[len++] = '|';
	memcpy(&dsocketptr->iod->dollar.key[len], socketptr->handle, socketptr->handle_len);
	len += socketptr->handle_len;
	dsocketptr->iod->dollar.key[len++] = '|';
	if (socket_local != socketptr->protocol)
		SPRINTF(&dsocketptr->iod->dollar.key[len], "%d", socketptr->local.port);
	else /* path goes in $key */
		strncpy(&dsocketptr->iod->dollar.key[len], ((struct sockaddr_un *)(socketptr->local.sa))->sun_path,
			DD_BUFLEN - len - 1);
	dsocketptr->iod->dollar.key[DD_BUFLEN - 1] = '\0';
	return TRUE;
}
