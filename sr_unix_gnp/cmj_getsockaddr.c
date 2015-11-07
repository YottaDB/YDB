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

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_ctype.h"

#include <errno.h>

#include "cmidef.h"
#include "eintr_wrappers.h"

#define IPV6_SEPARATOR ']'
#define IPV6_START '['

error_def(CMI_BADIPADDRPORT);
error_def(CMI_NOSERVENT);
error_def(ERR_TEXT);
error_def(CMI_NETFAIL);

/* writes into outport the TCP port cooresponding to tnd in NBO */
cmi_status_t cmj_getsockaddr(cmi_descriptor *nod, cmi_descriptor *tnd, struct addrinfo **ai_ptr_ptr)
{
	struct servent	*s;
	char		*tnd_str, *cp, *addr_str_ptr;
	char		addr_end;
	char		port_str[MAX_HOST_NAME_LEN], *port_str_ptr;
	char 		hn[MAX_HOST_NAME_LEN];
	int 		loop_limit = MAX_GETHOST_TRIES;
	int		tnd_len, iplen, port_len;
	struct addrinfo	hints, *ai_ptr = NULL;
	struct protoent *pe_ptr;
	int		errcode;
	int		port;

	/*
	 * address/port number determination:
	 *
	 * tnd := IP address:port for IPv4
	 * tnd := [IP address]:port for IPv6
	 *
	 * If tnd is an empty string, get port number from services database
	 *
	 */
	tnd_str = CMI_DESC_POINTER(tnd);
	tnd_len = CMI_DESC_LENGTH(tnd);
	if (tnd && (0 < tnd->len))
	{
		/* look for symbol separted ip address and port in ipv6 */
		if (IPV6_START == tnd_str[0])
		{
			if (NULL != (port_str_ptr = memchr(tnd_str, IPV6_SEPARATOR, tnd_len)))
			{
				if (0 == (iplen = (int)(port_str_ptr - &tnd_str[1])))
					return CMI_BADIPADDRPORT;
				if (iplen == tnd_len)
					return CMI_BADIPADDRPORT;
			} else
				return CMI_BADIPADDRPORT;
			addr_str_ptr = &tnd_str[1]; /* skip the '[' */
			iplen = port_str_ptr - addr_str_ptr;
			port_str_ptr += 1; /* skip the ']' */
			if (*port_str_ptr != ':')
				return CMI_BADIPADDRPORT;
			port_str_ptr += 1;
		} else
		{
			/* look for : */
			addr_str_ptr = port_str_ptr = tnd_str;
			for (iplen = 0, cp = tnd_str; cp < tnd_str + tnd_len; cp++)
			{
				if (*cp != ':' && *cp != '.' && !ISDIGIT_ASCII(*cp))
					return CMI_BADIPADDRPORT;
				if (*cp == ':')
				{
					if (0 == iplen)
					{
						if (0 == (iplen = (int)(cp - tnd_str)))
							return CMI_BADIPADDRPORT;
						port_str_ptr = (cp + 1);
					} else
						return CMI_BADIPADDRPORT;
				}
			}

		}
		port_len = (tnd_str + tnd_len) - port_str_ptr;
		if (0 >= port_len)
			return CMI_BADIPADDRPORT;
		memcpy(port_str, port_str_ptr, port_len);
		port_str[port_len] = '\0';

		if (0 != iplen)
		{	/* specified host name and port */
			addr_end = *(addr_str_ptr + iplen); /* the end can be ] or : */
			*(addr_str_ptr + iplen) = '\0';
			/* obtain ipv4/6 address for host tnd_str */
			CLIENT_HINTS(hints);
			if (0 != (errcode = getaddrinfo(addr_str_ptr, port_str, &hints, &ai_ptr)))
			{
				*(addr_str_ptr + iplen) = addr_end;
				return CMI_BADIPADDRPORT;
			}
			*(addr_str_ptr + iplen) = addr_end;
		} else
		{	/* only port is specified */
			if (NULL != nod)
			{
				assert(CMI_DESC_LENGTH(nod) < (SIZEOF(hn)-1));
				memcpy(hn, CMI_DESC_POINTER(nod), CMI_DESC_LENGTH(nod));
				hn[CMI_DESC_LENGTH(nod)] = '\0';
				CLIENT_HINTS(hints);

				while ((EAI_AGAIN == (errcode = getaddrinfo(hn, port_str, &hints, &ai_ptr))) && (0 < loop_limit))
				{
					loop_limit--;
				}
				if (0 != errcode)
					return CMI_NETFAIL;
				*ai_ptr_ptr = ai_ptr;
				return SS_NORMAL;
			}

			SERVER_HINTS(hints, (ipv4_only ? AF_INET : AF_UNSPEC));
			if (0 != (errcode = getaddrinfo(NULL, port_str, &hints, &ai_ptr)))
			{
				return CMI_BADIPADDRPORT;
			}
			iplen = -1;
		}
		errno = 0;
		port = ATOI(port_str);
		if ((0 == port) && (0 != errno) || (0 >= port))
			return CMI_BADIPADDRPORT;
	} else
	{	/* neither host nor port is specified */
		/* use the services db */
		SERVER_HINTS(hints, (ipv4_only ? AF_INET : AF_UNSPEC));
		if (0 != (errcode = getaddrinfo(NULL, GTCM_SERVER_NAME, &hints, &ai_ptr)))
		{
			return CMI_NOSERVENT;
		}
	}
	*ai_ptr_ptr = ai_ptr;
	return SS_NORMAL;
}
