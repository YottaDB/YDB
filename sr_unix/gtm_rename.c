/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_rename.h"
#include "iosp.h"

uint4 gtm_rename(char *org_fn, int org_fn_len, char *rename_fn, int rename_len, uint4 *ustatus)
{
	*ustatus = SS_NORMAL;	/* used in VMS only */
	assert(0 == org_fn[org_fn_len]);
	assert(0 == rename_fn[rename_len]);
	if (-1 == RENAME(org_fn, rename_fn))
		return errno;
	return SS_NORMAL;
}
