/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#elif (defined(__sparc) || defined (__ia64))
#define GTM_SOCKLEN_TYPE int
#else
#define GTM_SOCKLEN_TYPE socklen_t
#endif

#endif
