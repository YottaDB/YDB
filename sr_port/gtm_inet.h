/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#ifdef _AIX
#include <netinet/in.h>
#endif

#include <arpa/inet.h>

#ifdef NeedInAddrPort
typedef uint32_t	in_addr_t;
#endif

#define INET_ADDR	inet_addr
#define INET_NTOA	inet_ntoa

#endif
