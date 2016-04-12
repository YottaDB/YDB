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
#include "cmidef.h"
#include "gtm_netdb.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_inet.h"


error_def(CMI_BADPORT);
error_def(CMI_NOTND);
error_def(CMI_NETFAIL);
error_def(CMI_NOTTND);

/* writes into outport the TCP port cooresponding to tnd in NBO */
cmi_status_t cmj_get_port(cmi_descriptor *tnd, unsigned short *outport)
{
	cmi_status_t status = SS_NORMAL;
	char *envvar;
	char *tndstring;
	char *ep;
	char num[MAX_HOST_NAME_LEN];
	struct servent *s;
	unsigned short myport;

	/*
	 * port number determination:
	 *
	 * 1. Use string in tnd as an integer.
	 * 2. if the string is not an integer, use the
	 *    services database to get the port number
	 *
	 */
	if (tnd)
	{
		/* try to interpret tnd as an integer */
		memcpy(num, CMI_DESC_POINTER(tnd), CMI_DESC_LENGTH(tnd));
		num[CMI_DESC_LENGTH(tnd)] = '\0';
		myport = htons((unsigned short)STRTOUL(num, &ep, 10));
		if (*ep != '\0')
		{
			/* use the services db */
			s = getservbyname(num, GTCM_SERVER_PROTOCOL);
			endservent();
			if (!s)
				return CMI_NETFAIL;
			/* get port - netdb routine returns in NBO */
			myport = s->s_port;
		}
	}
	*outport = myport;
	return status;
}
