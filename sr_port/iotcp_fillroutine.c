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

/*  get socket routines address */
#include "mdef.h"

#include "gtm_netdb.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"

#include "io.h"
#include "iotcp_select.h"
#include "iotcproutine.h"

GBLDEF tcp_library_struct        tcp_routines;

int	gtm_accept(int socket, struct sockaddr *address, sssize_t *address_len);
int	gtm_connect(int socket, struct sockaddr *address, size_t address_len);
int	gtm_recv(int socket, void *buffer, size_t length, int flags);
int	gtm_send(int socket, void *buffer, size_t length, int flags);

/*
 * Note - the checks for EINTR in these functions are valid and need to stay in,
 * because the functions are being assigned to members of the tcp_routines table,
 * and thus cannot be replaced by EINTR wrapper macros.
 */

int	gtm_accept(int socket, struct sockaddr *address, sssize_t *address_len)
{
	int res;

	do
	{
		res = accept(socket, address, (GTM_SOCKLEN_TYPE *)address_len);
	} while (-1 == res && EINTR == errno);

	return(res);
}

int	gtm_connect(int socket, struct sockaddr *address, size_t address_len)
{
	int res;

	do
	{
		res = connect(socket, address, (GTM_SOCKLEN_TYPE)address_len);
	} while (-1 == res && EINTR == errno);

	return(res);
}

int	gtm_recv(int socket, void *buffer, size_t length, int flags)
{
	int res;

	do
	{
		res = (int)(recv(socket, buffer, (int)(length), flags));
	} while (-1 == res && EINTR == errno);

	return(res);
}

int	gtm_send(int socket, void *buffer, size_t length, int flags)
{
	int res;

	do
	{
		res = (int)(send(socket, buffer, (int)(length), flags));
	} while (-1 == res && EINTR == errno);

	return(res);
}

int	iotcp_fillroutine(void)
{
	tcp_routines.aa_accept = gtm_accept;
	tcp_routines.aa_bind = bind;
	tcp_routines.aa_close = close;
	tcp_routines.aa_connect = gtm_connect;
	tcp_routines.aa_getsockname = getsockname;
	tcp_routines.aa_getsockopt = getsockopt;
#ifndef htons       /* if htons is not a macro, assign the routine */
	tcp_routines.aa_htons = htons;
#endif
	tcp_routines.aa_inet_addr = INET_ADDR;
	tcp_routines.aa_inet_ntoa = INET_NTOA;
#ifndef ntohs
	tcp_routines.aa_ntohs = ntohs;
#endif
	tcp_routines.aa_listen = listen;
	tcp_routines.aa_recv = (int (*)())gtm_recv;
	tcp_routines.aa_select = select;
	tcp_routines.aa_send = (int (*)())gtm_send;
	tcp_routines.aa_setsockopt = setsockopt;
	tcp_routines.aa_socket = socket;

	return 0;
}
