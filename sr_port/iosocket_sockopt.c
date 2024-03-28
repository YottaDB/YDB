/****************************************************************
 *								*
 * Copyright (c) 2022 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "io.h"
#include "iosocketdef.h"
#include "gtmio.h"

error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);

int iosocket_getsockopt(socket_struct *socketptr, char *optname, int option, int level, void *optvalue,
	GTM_SOCKLEN_TYPE *optvaluelen, boolean_t freesocket)
{
	boolean_t		trap;
	char			*errptr, *sockopt_errptr;
	d_socket_struct		*dsocketptr;
	int4			errlen, real_errno;

	if (-1 == getsockopt(socketptr->sd, level, option, optvalue, optvaluelen))
	{
		real_errno = errno;
		dsocketptr = socketptr->dev;
		errptr = (char *)STRERROR(real_errno);
		trap = socketptr->ioerror;
		SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr, errlen);
		if (freesocket || trap)
		{
			if (FD_INVALID != socketptr->sd)
			{
				close(socketptr->sd);	/* Don't leave a dangling socket around */
				socketptr->sd = FD_INVALID;
			}
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
			      LEN_AND_STR(optname), real_errno, errlen, errptr);
		}
		return FALSE;
	}
	return TRUE;
}

int iosocket_setsockopt(socket_struct *socketptr, char *optname, int option, int level, void *optvalue,
	GTM_SOCKLEN_TYPE optvaluelen, boolean_t freesocket)
{
	boolean_t		trap;
	char			*errptr, *sockopt_errptr;
	d_socket_struct		*dsocketptr;
	int4			errlen, real_errno;

	if (-1 == setsockopt(socketptr->sd, level, option, optvalue, optvaluelen))
	{
		real_errno = errno;
		dsocketptr = socketptr->dev;
		errptr = (char *)STRERROR(real_errno);
		trap = socketptr->ioerror;
		SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr, errlen);
		if (freesocket || trap)
		{
			if (FD_INVALID != socketptr->sd)
			{
				close(socketptr->sd);	/* Don't leave a dangling socket around */
				socketptr->sd = FD_INVALID;
			}
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
			      LEN_AND_STR(optname), real_errno, errlen, errptr);
		}
		return FALSE;
	}
	return TRUE;
}
