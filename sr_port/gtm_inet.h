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

/* gtm_inet.h - interlude to <arpa/inet.h> system header file.  */
#ifndef GTM_INETH
#define GTM_INETH
#ifdef VMS
#include <inet/in.h>
#endif
#if defined(_AIX) || defined (__MVS__)
#include <netinet/in.h>
#endif

#include <arpa/inet.h>
#ifndef __MVS__
#include <netinet/tcp.h>
#endif

#ifdef NeedInAddrPort
typedef uint32_t	in_addr_t;
#endif

#define INET_ADDR	inet_addr
#define INET_NTOA	inet_ntoa

#endif
