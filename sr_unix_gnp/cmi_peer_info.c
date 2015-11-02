/****************************************************************
 *								*
 *	Copyright 2002, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#if defined(__MVS__) && !defined(_ISOC99_SOURCE)
#define _ISOC99_SOURCE
#endif

#include "mdef.h"
#include "cmidef.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_stdio.h"

/* return in a the char * the ASCII representation of a network address.
   Returned as:
   "hostid (nn.nn.nn.nn)" or "nn.nn.nn.nn" depending on whether or not
   the host is listed in /etc/hosts.
 */
void cmi_peer_info(struct CLB *lnk, char *buf, size_t sz)
{
    struct sockaddr_in *sin = &lnk->peer;
#ifndef SUNOSwhysmw
    struct hostent	*he;

    if ((he = gethostbyaddr((char *)&sin->sin_addr.s_addr,
			    SIZEOF(struct in_addr), AF_INET)))
	snprintf(buf, sz, "%s (%d.%d.%d.%d:%d)",he->h_name,
		   sin->sin_addr.s_addr >> 24,
		   sin->sin_addr.s_addr >> 16 & 0xFF,
		   sin->sin_addr.s_addr >> 8 & 0xFF,
		   sin->sin_addr.s_addr & 0xFF, (int)sin->sin_port);
    else
#endif
	snprintf(buf, sz, "%d.%d.%d.%d:%d",
		   sin->sin_addr.s_addr >> 24,
		   sin->sin_addr.s_addr >> 16 & 0xFF,
		   sin->sin_addr.s_addr >> 8 & 0xFF,
		   sin->sin_addr.s_addr & 0xFF, (int)sin->sin_port);
}
