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
#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"

GBLREF stack_frame *frame_pointer;

void	op_zcont(void)
{
	short		unwind;
	stack_frame	*fp;

	unwind = 1;
	for (fp = frame_pointer; fp ; fp = fp->old_frame_pointer, unwind++)
	{	if (fp->type & SFT_COUNT)
		{	return;
		}
		if (fp->type & SFT_DM)
		{	break;
		}
	}
	assert(fp);
	while (unwind-- > 0)
		op_unwind();
}/* eor */
