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

#include "gtm_string.h"
#include "gtm_uname.h"
#include "gtm_utsname.h"

int gtm_uname(char *name, int len)
{
	int	retval;
	struct utsname utsn;

	UNAME(&utsn, retval);
	(void)strncpy(name, utsn.nodename, (len < sizeof(utsn.nodename) ? len : sizeof(utsn.nodename)));

	return retval;
}
