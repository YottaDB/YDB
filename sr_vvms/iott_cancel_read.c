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
#include <iodef.h>
#include <ssdef.h>
#include "io.h"
#include "iottdef.h"
#include "efn.h"

void iott_cancel_read(io_ptr)
io_desc *io_ptr;

/* This routine should always be followed by an iott_resetast, which
includes a qiow that we would prefer not to do within the AST */
{
	uint4 	status;
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct*)(io_ptr->dev_sp);
	if (!tt_ptr->stat_blk.status)
	{
#ifdef DEBUG
/* this is for an assert that verifies a reliance on VMS IOSB maintenance */
		tt_ptr->read_timer = FALSE;
#endif
		if ((status = sys$cancel(tt_ptr->channel)) != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
	}
	return;
}
