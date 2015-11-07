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

/* iosocket_create.c */
/* this module takes care of
 *	1. allocate the space
 *	2. for passive: local.sa & local.ai
 *	   for active : remote.sa & remote.ai
 *	   for $principal: via getsockname and getsockpeer
 *	3. socketptr->protocol
 *	4. socketptr->sd (initialized to -1) unless already open via inetd
 *	5. socketptr->passive
 *	6. socketptr->state (initialized to created) unless already open
 */
#include "mdef.h"

#include <errno.h>
#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_ipv6.h"
#include "gtm_stdlib.h"

#include "io.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "gtm_caseconv.h"
#include "util.h"

GBLREF	tcp_library_struct	tcp_routines;

error_def(ERR_GETSOCKNAMERR);
error_def(ERR_GETADDRINFO);
error_def(ERR_GETNAMEINFO);
error_def(ERR_INVPORTSPEC);
error_def(ERR_INVADDRSPEC);
error_def(ERR_PROTNOTSUP);
error_def(ERR_TEXT);
error_def(ERR_SOCKINIT);

/* PORT_PROTO_FORMAT defines the format for <port>:<protocol> */
#define PORT_PROTO_FORMAT "%hu:%3[^:]"
#define	SEPARATOR ':'

socket_struct *iosocket_create(char *sockaddr, uint4 bfsize, int file_des)
{
	socket_struct		*socketptr;
	socket_struct		*prev_socketptr;
	socket_struct		*socklist_head;
	bool			passive = FALSE;
	unsigned short		port;
	int			ii, save_errno, tmplen, errlen;
	char 			temp_addr[SA_MAXLITLEN], tcp[4], *adptr;
	const char		*errptr;
	struct addrinfo		*ai_ptr;
	struct addrinfo		hints, *addr_info_ptr = NULL;
	int			af;
	int			sd;
	int			errcode;
	char			port_buffer[NI_MAXSERV];
	int			port_buffer_len;
	int			colon_cnt;
	char			*last_2colon;
	int			addrlen;
	GTM_SOCKLEN_TYPE	tmp_addrlen;

	if (0 > file_des)
	{	/* no socket descriptor yet */
		memset(&hints, 0, SIZEOF(hints));

		colon_cnt = 0;
		for (ii = strlen(sockaddr) - 1; 0 <= ii; ii--)
		{
			if (SEPARATOR == sockaddr[ii])
			{
				colon_cnt++;
				if (2 == colon_cnt)
				{
					last_2colon = &sockaddr[ii];
					break;
				}
			}
		}
		if (0 == colon_cnt)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVPORTSPEC);
			return NULL;
		}
		if (1 == colon_cnt)
		{	/* for listening socket or broadcasting socket */
			if (SSCANF(sockaddr, PORT_PROTO_FORMAT, &port, tcp) < 2)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVPORTSPEC);
				return NULL;
			}
			passive = TRUE;
			/* We always first try using IPv6 address, if supported */
			af = ((GTM_IPV6_SUPPORTED && !ipv4_only) ? AF_INET6 : AF_INET);
			if (-1 == (sd = tcp_routines.aa_socket(af, SOCK_STREAM, IPPROTO_TCP)))
			{
				/* Try creating IPv4 socket */
				af = AF_INET;
				if (-1 == (sd = tcp_routines.aa_socket(af, SOCK_STREAM, IPPROTO_TCP)))
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					errlen = STRLEN(errptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, save_errno, errlen, errptr);
					return NULL;
				}
			}
			SERVER_HINTS(hints, af);
			port_buffer_len = 0;
			I2A(port_buffer, port_buffer_len, port);
			port_buffer[port_buffer_len]='\0';
			if (0 != (errcode = getaddrinfo(NULL, port_buffer, &hints, &addr_info_ptr)))
			{
				tcp_routines.aa_close(sd);
				RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
				return NULL;
			}
			SOCKET_ALLOC(socketptr);
			socketptr->local.port = port;
			socketptr->temp_sd = sd;
			socketptr->sd = FD_INVALID;
			ai_ptr = &(socketptr->local.ai);
			memcpy(ai_ptr, addr_info_ptr, SIZEOF(struct addrinfo));
			SOCKET_AI_TO_LOCAL_ADDR(socketptr, addr_info_ptr);
			ai_ptr->ai_addr = SOCKET_LOCAL_ADDR(socketptr);
			ai_ptr->ai_addrlen = addr_info_ptr->ai_addrlen;
			ai_ptr->ai_next = NULL;
			freeaddrinfo(addr_info_ptr);
		} else
		{	/* connection socket */
			assert(2 == colon_cnt);
			if (SSCANF(last_2colon + 1, PORT_PROTO_FORMAT, &port, tcp) < 2)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVPORTSPEC);
				return NULL;
			}
			/* for connection socket */
			SPRINTF(port_buffer, "%hu", port);
			addrlen = last_2colon - sockaddr;
			if ('[' == sockaddr[0])
			{
				if (NULL == memchr(sockaddr, ']', addrlen))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVPORTSPEC);
					return NULL;
				}
				addrlen -= 2;
				memcpy(temp_addr, &sockaddr[1], addrlen);
			} else
				memcpy(temp_addr, sockaddr, addrlen);
			temp_addr[addrlen] = 0;
			CLIENT_HINTS(hints);
			if (0 != (errcode = getaddrinfo(temp_addr, port_buffer, &hints, &addr_info_ptr)))
			{
				RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
				return NULL;
			}
			/*  we will test all address families in iosocket_connect() */
			SOCKET_ALLOC(socketptr);
			socketptr->remote.ai_head = addr_info_ptr;
			socketptr->remote.port = port;
			socketptr->sd = socketptr->temp_sd = FD_INVALID; /* don't mess with 0 */
		}
		lower_to_upper((uchar_ptr_t)tcp, (uchar_ptr_t)tcp, SIZEOF("TCP") - 1);
		if (0 == MEMCMP_LIT(tcp, "TCP"))
		{
			socketptr->protocol = socket_tcpip;
		} else
		{
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PROTNOTSUP, 2, MIN(strlen(tcp), SIZEOF("TCP") - 1), tcp);
			return NULL;
		}
		socketptr->state = socket_created;
		SOCKET_BUFFER_INIT(socketptr, bfsize);
		socketptr->passive = passive;
		socketptr->moreread_timeout = DEFAULT_MOREREAD_TIMEOUT;

		return socketptr;
	} else
	{	/* socket already setup by inetd */
		SOCKET_ALLOC(socketptr);
		socketptr->sd = file_des;
		socketptr->temp_sd = FD_INVALID;
		ai_ptr = &(socketptr->local.ai);
		tmp_addrlen = SIZEOF(struct sockaddr_storage);
		if (-1 == tcp_routines.aa_getsockname(socketptr->sd, SOCKET_LOCAL_ADDR(socketptr), &tmp_addrlen))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			tmplen = STRLEN(errptr);
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, save_errno, tmplen, errptr);
			return NULL;
		}
		ai_ptr->ai_addrlen = tmp_addrlen;
		/* extract port information */
		GETNAMEINFO(SOCKET_LOCAL_ADDR(socketptr), tmp_addrlen, NULL, 0, port_buffer, NI_MAXSERV, NI_NUMERICSERV, errcode);
		if (0 != errcode)
		{
			SOCKET_FREE(socketptr);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return NULL;
		}
		socketptr->local.port = ATOI(port_buffer);
		tmp_addrlen = SIZEOF(struct sockaddr_storage);
		if (-1 == getpeername(socketptr->sd, SOCKET_REMOTE_ADDR(socketptr), &tmp_addrlen))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			tmplen = STRLEN(errptr);
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, save_errno, tmplen, errptr);
			return NULL;
		}
		socketptr->remote.ai.ai_addrlen = tmp_addrlen;
		assert(0 != SOCKET_REMOTE_ADDR(socketptr)->sa_family);
		GETNAMEINFO(SOCKET_REMOTE_ADDR(socketptr), socketptr->remote.ai.ai_addrlen, NULL, 0, port_buffer, NI_MAXSERV,
				NI_NUMERICSERV, errcode);
		if (0 != errcode)
		{
			SOCKET_FREE(socketptr);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return NULL;
		}
		socketptr->remote.port = ATOI(port_buffer);
		socketptr->state = socket_connected;
		socketptr->protocol = socket_tcpip;
		SOCKET_BUFFER_INIT(socketptr, bfsize);
		socketptr->passive = passive;
		socketptr->moreread_timeout = DEFAULT_MOREREAD_TIMEOUT;
		return socketptr;
	}
}
