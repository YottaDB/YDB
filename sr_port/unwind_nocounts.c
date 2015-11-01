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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "op.h"
#include "unwind_nocounts.h"

GBLREF stack_frame	*frame_pointer;
GBLREF tp_frame		*tp_pointer;

bool unwind_nocounts(void)
{	short		unwind;
	bool		dmode;
	stack_frame	*fp;
	error_def(ERR_TPQUIT);

	dmode = FALSE;
	unwind = 0;

	for (fp = frame_pointer; !(fp->type & SFT_COUNT) && fp->old_frame_pointer; fp = fp->old_frame_pointer)
	{	if (fp->type & SFT_DM)
		{	dmode = TRUE;
		}
		unwind++;
	}

	if (tp_pointer && tp_pointer->fp < fp)
		rts_error(VARLSTCNT(1) ERR_TPQUIT);

	while (unwind-- > 0)
		op_unwind();

	return(dmode);
}
