/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
 *	3. socketptr->protocol
 *	4. socketptr->sd (initialized to -1)
 *	5. socketptr->passive
 *	6. socketptr->state (initialized to created)
 */
#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"

#include "io.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "gtm_caseconv.h"

GBLREF	tcp_library_struct	tcp_routines;
socket_struct *iosocket_create(char *sockaddr, uint4 bfsize)
{
	socket_struct	*socketptr;
	bool		passive = FALSE;
	unsigned short	port;
	int		ii, save_errno;
	char 		temp_addr[SA_MAXLITLEN], addr[SA_MAXLEN], tcp[4], *errptr;
	error_def(ERR_INVPORTSPEC);
	error_def(ERR_INVADDRSPEC);
	error_def(ERR_PROTNOTSUP);
	error_def(ERR_TEXT);
	socketptr = (socket_struct *)malloc(sizeof(socket_struct));
	memset(socketptr, 0, sizeof(socket_struct));
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
	}
	else
	{
		for (ii = 0; ISDIGIT(temp_addr[ii]) || '.' == temp_addr[ii]; ii++)
			;
		if (temp_addr[ii] != '\0')
		{
			SPRINTF(socketptr->remote.saddr_lit, "%s", temp_addr);
			SPRINTF(addr, "%s", iotcp_name2ip(temp_addr));
		}
		else
			SPRINTF(addr, "%s", temp_addr);
		if (-1 == (socketptr->remote.sin.sin_addr.s_addr = tcp_routines.aa_inet_addr(addr)))
		{
			errptr = (char *)STRERROR(errno);
			free(socketptr);
			rts_error(VARLSTCNT(6) ERR_INVADDRSPEC, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
			return NULL;
		}
		socketptr->remote.sin.sin_port = GTM_HTONS(port);
		socketptr->remote.sin.sin_family = AF_INET;
		socketptr->remote.port = port;
		SPRINTF(socketptr->remote.saddr_ip, "%s", addr);
	}
	lower_to_upper((uchar_ptr_t)tcp, (uchar_ptr_t)tcp, sizeof("TCP") - 1);
	if (0 == MEMCMP_LIT(tcp, "TCP"))
		socketptr->protocol = socket_tcpip;
	else
	{
		free(socketptr);
		rts_error(VARLSTCNT(4) ERR_PROTNOTSUP, 2, MIN(strlen(tcp), sizeof("TCP") - 1), tcp);
		return NULL;
	}
	socketptr->buffer = (char *)malloc(bfsize);
	socketptr->buffer_size = bfsize;
	socketptr->buffered_length = socketptr->buffered_offset = 0;
	socketptr->sd = -1; /* don't mess with 0 */
	socketptr->passive = passive;
	socketptr->state = socket_created; /* Is this really useful? */
	return socketptr;
}
