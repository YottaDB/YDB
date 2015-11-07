/****************************************************************
 *								*
 *	Copyright 2002, 2013 Fidelity Information Services, Inc	*
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

#include <errno.h>	/* for errno, used by RTS_ERROR_ADDRINFO */

#include "mdef.h"
#include "cmidef.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_stdio.h"
#include "iotcpdef.h"
#include "gtm_string.h"	/* for rts_error */

/* return in a the char * the ASCII representation of a network address.
   Returned as:
   "hostid (nn.nn.nn.nn)" or "nn.nn.nn.nn" depending on whether or not
   the host is listed in /etc/hosts.
 */
error_def(ERR_GETNAMEINFO);
error_def(ERR_TEXT);

void cmi_peer_info(struct CLB *lnk, char *buf, size_t sz)
{
	struct addrinfo *ai_ptr;
	char 		hostname[SA_MAXLITLEN];
	char 		ipname[SA_MAXLEN];
	char		port_str[NI_MAXSERV];
	int		errcode;

	ai_ptr = &lnk->peer_ai;
	if (0 != (errcode = getnameinfo(ai_ptr->ai_addr, ai_ptr->ai_addrlen,
					 ipname, SA_MAXLEN,
					 port_str, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV)))
	{
		assert(0);
		RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
		return;
	}
	if (0 != (errcode = getnameinfo(ai_ptr->ai_addr, ai_ptr->ai_addrlen,
					 hostname, SA_MAXLITLEN,
					 NULL, 0, 0)))
	{
		assert(0);
		RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
		return;
	}
	SNPRINTF(buf, sz, "%s (%s:%s)", hostname, ipname, port_str);
}
