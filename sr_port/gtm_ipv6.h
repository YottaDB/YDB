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

/* gtm_ipv6.h - interlude to <sys/socket.h> system header file.  */
#ifndef GTM_IPV6H
#define GTM_IPV6H

#include <gtm_netdb.h>		/* Make sure we have AI_V4MAPPED/AI_NUMERICSERV defined if available */

GBLREF	boolean_t	ipv4_only;	/* If TRUE, only use AF_INET. */

/* ai_canonname must be set NULL for AIX. Otherwise, freeaddrinfo() freeing the ai_canonname will hit SIG-11
 * other field which were not initialized as 0 will also causes getaddrinfo()to fail
 * Setting AI_PASSIVE will give you a wildcard address if addr is NULL, i.e. INADDR_ANY or IN6ADDR_ANY
 * AI_NUMERICSERV is to pass the numeric port to address, it is to inhibit the name resolution to improve efficience
 * AI_ADDRCONFIG: IPv4 addresses are returned only if  the local system has at least one IPv4 address
		  configured; IPv6 addresses are only returned if the local system has at least one IPv6
		  address configured. For now we only use IPv6 address. So not use this flag here.
 * AI_V4MAPPED:  IPv4 mapped addresses are acceptable
 * (Note: for snail, AI_V4MAPPED is defined but AI_NUMERICSERV is not defined)
 */

#if (defined(__hppa) || defined(__vms) || defined(__osf__))
#define GTM_IPV6_SUPPORTED	FALSE
#else
#define GTM_IPV6_SUPPORTED	TRUE
#endif

#if	!GTM_IPV6_SUPPORTED
#define SERVER_HINTS(hints, af)						\
{									\
	assert(AF_INET6 != af);						\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = AF_INET;					\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags =  AI_PASSIVE;					\
}
#define CLIENT_HINTS(hints)						\
{									\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = AF_INET;					\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags = 0;						\
}

#define CLIENT_HINTS_AF(hints, af)					\
{									\
	assert(AF_INET == af);						\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = AF_INET;					\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags = 0;						\
}

#elif (defined(AI_V4MAPPED) && defined(AI_NUMERICSERV))
#define SERVER_HINTS(hints, af)						\
{									\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = af;						\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags = AI_V4MAPPED | AI_PASSIVE | AI_NUMERICSERV;	\
}
#define CLIENT_HINTS(hints)						\
{									\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = (ipv4_only ? AF_INET : AF_UNSPEC);		\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;			\
}
#define CLIENT_HINTS_AF(hints, af)					\
{									\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = af;						\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags = AI_V4MAPPED;					\
}

#else
#error "Ok, so we do have non-AI_V4MAPPED/AI_NUMERICSERV machines with IPv6 support"
#define SERVER_HINTS(hints, af)						\
{									\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = af;						\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags =  AI_PASSIVE;					\
}
#define CLIENT_HINTS(hints)						\
{									\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = (ipv4_only ? AF_INET : AF_UNSPEC);		\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags = AI_ADDRCONFIG;					\
}
#define CLIENT_HINTS_AF(hints, af)					\
{									\
	memset(&hints, 0, SIZEOF(struct addrinfo));			\
	hints.ai_family = AF_INET;					\
	hints.ai_socktype = SOCK_STREAM;				\
	hints.ai_protocol = IPPROTO_TCP;				\
	hints.ai_flags = 0;						\
}
#endif

#define FREEADDRINFO(ai_ptr)						\
{									\
	if(ai_ptr)							\
		freeaddrinfo(ai_ptr);					\
}

union gtm_sockaddr_in46
{
	struct sockaddr_in  ipv4;
#	if GTM_IPV6_SUPPORTED
	struct sockaddr_in6 ipv6;
#	endif
};

#endif
