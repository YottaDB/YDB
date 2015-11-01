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

#include <sys/socket.h>
#include "gtm_netdb.h"
#include <netinet/in.h>
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "io.h"


char *iotcp_name2ip(char *name)
{
  struct   hostent 	host, *host_ptr;
  struct   in_addr 	inaddr;
  char     		local_name[80];
  char     		ip[16];

  SPRINTF(local_name, "%s", name);

  if (NULL == (host_ptr = GETHOSTBYNAME(local_name)))
      return NULL;

  host = *host_ptr;

  inaddr = *((struct in_addr *)host.h_addr_list[0]);

  return INET_NTOA(inaddr);
}
