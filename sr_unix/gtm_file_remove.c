/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "gtm_file_remove.h"
#include "iosp.h"

int4 gtm_file_remove(char *fn, int fn_len, uint4 *ustatus)
{
	assert(0== fn[fn_len]);
	*ustatus = SS_NORMAL;	/* used in VMS only */
	if (-1 == UNLINK(fn))
		return errno;
	return SS_NORMAL;
}
