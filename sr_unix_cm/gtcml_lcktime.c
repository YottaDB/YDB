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

#include "mdef.h"

#include <errno.h>

#include "cmidef.h"
#include "cmmdef.h"
#include "mlkdef.h"
#include "gtcmlkdef.h"

boolean_t	gtcml_lcktime(cm_lckblklck *lck)
{
	unsigned int time_now;
	error_def(CMERR_CMSYSSRV);

	if (-1 == (time_now = time(NULL)))
		rts_error(VARLSTCNT(3) CMERR_CMSYSSRV, 0, errno);
	return((time_now - lck->blktime[0]) < 16 ? FALSE : TRUE);
}
