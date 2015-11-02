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

/* gtm_socket.h - interlude to <sys/socket.h> system header file.  */
#ifndef GTM_SOCKETH
#define GTM_SOCKETH

#include <sys/socket.h>

#define BIND		bind
#define CONNECT		connect
#define ACCEPT		accept
#define RECVFROM	recvfrom
#define SENDTO		sendto

#if defined(__osf__) && defined(__alpha)
#define GTM_SOCKLEN_TYPE size_t
#elif defined(VMS)
#define GTM_SOCKLEN_TYPE size_t
#elif defined(__sparc)
#define GTM_SOCKLEN_TYPE int
#else
#define GTM_SOCKLEN_TYPE socklen_t
#endif

#ifdef GTM_FD_TRACE
/* Just like open and close were noted down in gtm_fcntl.h, note down all macros which we are redefining here and could
 * potentially have been conflictingly defined by the system header file "socket.h". The system define will be used
 * in gtm_fd_trace.c within the implementation of the GT.M interlude function. Currently none of these functions (socket)
 * are defined by the system so it is not theoretically necessary but they could be defined in the future.
 */
#	undef	socket			/* in case this is already defined by <socket.h> */
#	define	socket	gtm_socket
#endif

int gtm_socket(int domain, int type, int protocol);
int gtm_connect(int socket, struct sockaddr *address, size_t address_len);

#endif
