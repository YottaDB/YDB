/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "cmihdr.h"
#include "cmidef.h"

void cmj_disconn2(lnk)
struct CLB *lnk;
{
	int status;
	error_def(CMI_NETFAIL);

/* Ignore iosb of previous qio because we're going to deassign the link regardless.  */

	status = SYS$DASSGN(lnk->dch);
	lib$free_vm(&SIZEOF(*lnk), &lnk, 0);
	if ((status & 1) == 0)
		rts_error(CMI_NETFAIL,0,status);
	return;
}
