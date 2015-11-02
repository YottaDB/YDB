/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
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

GBLREF stack_frame	*frame_pointer;

void adjust_frames(unsigned char *old_ptext_beg, unsigned char *old_ptext_end, unsigned char *new_ptext_beg)
{
	stack_frame	*fp;

	for (fp = frame_pointer; NULL != fp; fp = fp->old_frame_pointer)
	{
		if (old_ptext_beg <= fp->mpc && fp->mpc <= old_ptext_end)
			fp->mpc += (new_ptext_beg - old_ptext_beg);
	}
	return;
}
