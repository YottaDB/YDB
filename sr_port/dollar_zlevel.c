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
#include "dollar_zlevel.h"

GBLREF stack_frame	*frame_pointer;

int dollar_zlevel(void)
{
	int		count;
       	stack_frame	*fp;

	for (count = 0, fp = frame_pointer; fp->old_frame_pointer; fp = fp->old_frame_pointer)
	{
		if (!(fp->type & SFT_COUNT))
			continue;
		count++;
	}
	assert(0 < count); /* $ZLEVEL should be atleast 1 */
	return (count);
}
