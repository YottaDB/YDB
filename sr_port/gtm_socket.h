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

/* gtm_socket.h - interlude to <sys/socket.h> system header file.  */
#ifndef GTM_SOCKETH
#define GTM_SOCKETH

#ifdef VMS
#include <socket.h>
#else
#include <sys/socket.h>
#endif

#define BIND		bind
#define CONNECT		connect
#define ACCEPT		accept
#define RECVFROM	recvfrom
#define SENDTO		sendto
typedef struct sockaddr	*sockaddr_ptr;

#if defined(__osf__) && defined(__alpha)
#define GTM_SOCKLEN_TYPE size_t
#elif defined(VMS)
#define GTM_SOCKLEN_TYPE size_t
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

#if defined(VMS) && !defined(_SS_PAD2SIZE)
/* No sockaddr_storage on OpenVMS 7.2-1, but we only support AF_INET on VMS, so use sockaddr_in. */
#define	sockaddr_storage sockaddr_in
/* getnameinfo() inexplicably throws an ACCVIO/NOPRIV on OpenVMS 7.2-1, so revert to the old API.  */
#define GETNAMEINFO(SA, SALEN, HOST, HOSTLEN, SERV, SERVLEN, FLAGS, RES)			\
{												\
	assert(((struct sockaddr *)(SA))->sa_family == AF_INET);				\
	assert((FLAGS & NI_NUMERICHOST) || (NULL == HOST));					\
	assert((FLAGS & NI_NUMERICSERV) || (NULL == SERV));					\
	assert(FLAGS & (NI_NUMERICHOST | NI_NUMERICSERV));					\
	if ((FLAGS & NI_NUMERICHOST) && (NULL != HOST))						\
		STRNCPY(HOST, inet_ntoa(((struct sockaddr_in *)(SA))->sin_addr), HOSTLEN);	\
	if ((FLAGS & NI_NUMERICSERV) && (NULL != SERV))						\
		i2asc((uchar_ptr_t)(SERV), ntohs(((struct sockaddr_in *)(SA))->sin_port));	\
	RES = 0;										\
}
#else
#define GETNAMEINFO(SA, SALEN, HOST, HOSTLEN, SERV, SERVLEN, FLAGS, RES)		\
{											\
	RES = getnameinfo(SA, SALEN, HOST, HOSTLEN, SERV, SERVLEN, FLAGS);		\
}
#endif

#endif
