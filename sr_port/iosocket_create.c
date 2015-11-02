/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
 *	2. for passive: local.sin.sin_addr.s_addr & local.sin.sin_port
 *	   for active : remote.sin.sin_addr.s_addr & remote.sin.sin_port
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

#include "io.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "gtm_caseconv.h"

#ifdef __osf__
/* Tru64 does not have the prototype for "hstrerror" even though the function is available in the library.
 * Until we revamp the TCP communications setup stuff to use the new(er) POSIX definitions, we cannot move
 * away from "hstrerror". Declare prototype for this function in Tru64 manually until then.
 */
const char *hstrerror(int err);
#endif

GBLREF	tcp_library_struct	tcp_routines;

socket_struct *iosocket_create(char *sockaddr, uint4 bfsize, int file_des)
{
	socket_struct	*socketptr;
	bool		passive = FALSE;
	unsigned short	port;
	int		ii, save_errno, tmplen;
	GTM_SOCKLEN_TYPE	socknamelen;
	char 		temp_addr[SA_MAXLITLEN], addr[SA_MAXLEN], tcp[4], *adptr;
	const char	*errptr;

	error_def(ERR_INVPORTSPEC);
	error_def(ERR_INVADDRSPEC);
	error_def(ERR_PROTNOTSUP);
	error_def(ERR_TEXT);
	error_def(ERR_GETSOCKNAMERR);

	socketptr = (socket_struct *)malloc(SIZEOF(socket_struct));
	memset(socketptr, 0, SIZEOF(socket_struct));
	if (0 > file_des)
	{	/* no socket descriptor yet */
		if (SSCANF(sockaddr, "%[^:]:%hu:%3[^:]", temp_addr, &port, tcp) < 3)
		{
			passive = TRUE;
			socketptr->local.sin.sin_addr.s_addr = INADDR_ANY;
			if(SSCANF(sockaddr, "%hu:%3[^:]", &port, tcp) < 2)
			{
				free(socketptr);
				rts_error(VARLSTCNT(1) ERR_INVPORTSPEC);
				return NULL;
			}
			socketptr->local.sin.sin_port = GTM_HTONS(port);
			socketptr->local.sin.sin_family = AF_INET;
			socketptr->local.port = port;
		} else
		{
			for (ii = 0; ISDIGIT_ASCII(temp_addr[ii]) || '.' == temp_addr[ii]; ii++) /* NOTE: only ASCII digits */
				;							   /* allowed for dotted notation address */
			if (temp_addr[ii] != '\0')
			{
				SPRINTF(socketptr->remote.saddr_lit, "%s", temp_addr);
				adptr = iotcp_name2ip(temp_addr);
				if (NULL == adptr)
				{
#if !defined(__hpux) && !defined(__MVS__)
					errptr = HSTRERROR(h_errno);
					rts_error(VARLSTCNT(6) ERR_INVADDRSPEC, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
#else
					/* Grumble grumble HPUX and z/OS don't have hstrerror() */
					rts_error(VARLSTCNT(1) ERR_INVADDRSPEC);
#endif
					free(socketptr);
					return NULL;
				}

				SPRINTF(addr, "%s", adptr);
			} else
				SPRINTF(addr, "%s", temp_addr);
			if ((unsigned int)-1 == (socketptr->remote.sin.sin_addr.s_addr = tcp_routines.aa_inet_addr(addr)))
			{	/* Errno not set by inet_addr() */
				free(socketptr);
				rts_error(VARLSTCNT(1) ERR_INVADDRSPEC);
				return NULL;
			}
			socketptr->remote.sin.sin_port = GTM_HTONS(port);
			socketptr->remote.sin.sin_family = AF_INET;
			socketptr->remote.port = port;
			SPRINTF(socketptr->remote.saddr_ip, "%s", addr);
		}
		lower_to_upper((uchar_ptr_t)tcp, (uchar_ptr_t)tcp, SIZEOF("TCP") - 1);
		if (0 == MEMCMP_LIT(tcp, "TCP"))
			socketptr->protocol = socket_tcpip;
		else
		{
			free(socketptr);
			rts_error(VARLSTCNT(4) ERR_PROTNOTSUP, 2, MIN(strlen(tcp), SIZEOF("TCP") - 1), tcp);
			return NULL;
		}
		socketptr->sd = FD_INVALID; /* don't mess with 0 */
		socketptr->state = socket_created; /* Is this really useful? */
	} else
	{	/* socket already setup by inetd */
		socketptr->sd = file_des;
		socknamelen = SIZEOF(socketptr->local.sin);
		if (-1 == tcp_routines.aa_getsockname(socketptr->sd, (struct sockaddr *)&socketptr->local.sin, &socknamelen))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			tmplen = STRLEN(errptr);
			rts_error(VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, save_errno, tmplen, errptr);
			free(socketptr);
			return NULL;
		}
		socketptr->local.port = GTM_NTOHS(socketptr->local.sin.sin_port);
		socknamelen = SIZEOF(socketptr->remote.sin);
		if (-1 == getpeername(socketptr->sd, (struct sockaddr *)&socketptr->remote.sin, (GTM_SOCKLEN_TYPE *)&socknamelen))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			tmplen = STRLEN(errptr);
			rts_error(VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, save_errno, tmplen, errptr); /* need new error */
			free(socketptr);
			return NULL;
		}
		socketptr->remote.port = GTM_NTOHS(socketptr->remote.sin.sin_port);
		socketptr->state = socket_connected;
		socketptr->protocol = socket_tcpip;
	}
	socketptr->buffer = (char *)malloc(bfsize);
	socketptr->buffer_size = bfsize;
	socketptr->buffered_length = socketptr->buffered_offset = 0;
	socketptr->passive = passive;
	socketptr->moreread_timeout = DEFAULT_MOREREAD_TIMEOUT;
	return socketptr;
}
