/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_ctype.h"

#include <errno.h>

#include "cmidef.h"
#include "eintr_wrappers.h"

/* writes into outport the TCP port cooresponding to tnd in NBO */
cmi_status_t cmj_getsockaddr(cmi_descriptor *tnd, struct sockaddr_in *inp)
{
	struct servent	*s;
	char		*tnd_str, *cp;
	char		port_str[MAX_HOST_NAME_LEN];
	int		tnd_len, ip_len;

	error_def(CMI_BADIPADDRPORT);
	error_def(CMI_NOSERVENT);

	/*
	 * address/port number determination:
	 *
	 * tnd := [IP address:]port
	 *
	 * If tnd is an empty string, get port number from services database
	 *
	 */
	tnd_str = CMI_DESC_POINTER(tnd);
	tnd_len = CMI_DESC_LENGTH(tnd);
	if (tnd && 0 < tnd->len)
	{
		/* look for : */
		for (ip_len = 0, cp = tnd_str; cp < tnd_str + tnd_len; cp++)
		{
			if (*cp != ':' && *cp != '.' && !ISDIGIT_ASCII(*cp))
				return CMI_BADIPADDRPORT;
			if (*cp == ':')
			{
				if (0 == ip_len)
				{
					if (0 == (ip_len = (int)(cp - tnd_str)))
						return CMI_BADIPADDRPORT;
				} else
					return CMI_BADIPADDRPORT;
			}
		}
		if (0 != ip_len)
		{
			*(tnd_str + ip_len) = '\0';
			inp->sin_addr.s_addr = INET_ADDR(tnd_str);
			*(tnd_str + ip_len) = ':';
			if ((in_addr_t)-1 == inp->sin_addr.s_addr)
				return CMI_BADIPADDRPORT;
		} else
		{
			inp->sin_addr.s_addr = INADDR_ANY;
			ip_len = -1;
		}
		if (tnd_len - ip_len > SIZEOF(port_str))
			return CMI_BADIPADDRPORT;
		memcpy(port_str, tnd_str + ip_len + 1, tnd_len - ip_len - 1);
		port_str[tnd_len - ip_len - 1] = '\0';
		errno = 0;
		if (((0 == (inp->sin_port = ATOI(port_str))) && (0 != errno)) || (0 >= inp->sin_port))
			return CMI_BADIPADDRPORT;
		inp->sin_port = htons(inp->sin_port);
	} else
	{
		inp->sin_addr.s_addr = INADDR_ANY;
		/* use the services db */
		s = getservbyname(GTCM_SERVER_NAME, GTCM_SERVER_PROTOCOL);
		endservent();
		if (!s)
			return CMI_NOSERVENT;
		/* get port - netdb routine returns in NBO */
		inp->sin_port = s->s_port;
	}
	inp->sin_family = AF_INET;
	return SS_NORMAL;
}
