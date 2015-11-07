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
#include <ssdef.h>
#include "efn.h"
#include "io.h"
#include "iombdef.h"

void iomb_cancel_read(mb_ptr)
d_mb_struct *mb_ptr;
{
	uint4	status;

	if (!mb_ptr->stat_blk.status)
	{
#ifdef DEBUG
/* this is for an assert that verifies a reliance on VMS IOSB maintenance */
		mb_ptr->timer_on = FALSE;
#endif
		status = sys$cancel(mb_ptr->channel);
		if (status  != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
	}
	return;
}
