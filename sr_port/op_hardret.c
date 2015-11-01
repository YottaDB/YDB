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
#include "op.h"
#include "unwind_nocounts.h"
#include "dm_setup.h"

GBLREF stack_frame	*frame_pointer;

void	op_hardret(void)
{
	bool		dmode;

	dmode = unwind_nocounts();
	assert(frame_pointer->old_frame_pointer);
	if (frame_pointer->old_frame_pointer->old_frame_pointer)
	{	op_unwind();
	}
	if (dmode)
	{	dm_setup();
	}
	return;
}/* eor */
