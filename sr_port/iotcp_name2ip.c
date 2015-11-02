/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iotcp_name2ip.c     - convert a host name to its ip address
 *
 *  Parameters-
 *      name           - the pointer to the string of hostname.
 *
 *  Returns-
 *      ip             - the pointer to the string of ip address.
 *      NULL           - convert operation failed, host not reachable.
 */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "io.h"

char *iotcp_name2ip(char *name)
{ /* only ASCII range characters allowed in hostnames, we are not using libidn to resolve hostnames that might have international
     characters in their name */

	struct   hostent	*host_ptr;

	if (NULL == (host_ptr = GETHOSTBYNAME(name)))
		return NULL;
	return INET_NTOA(*((struct in_addr *)host_ptr->h_addr_list[0]));
}
