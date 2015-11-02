/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "dollar_zlevel.h"

GBLREF stack_frame	*frame_pointer;

int dollar_zlevel()
{
	int		count;
       	stack_frame	*fp, *fpprev;

	for (count = 0, fp = frame_pointer; NULL != fp; fp = fpprev)
	{
		assert((fp < fp->old_frame_pointer) || (NULL == fp->old_frame_pointer));
		fpprev = fp->old_frame_pointer;
		if (!(fp->type & SFT_COUNT))
			continue;
		if (NULL == fpprev)
		{	/* Next frame is some sort of base frame */
#			ifdef GTM_TRIGGER
			if (fp->type & SFT_TRIGR)
			{	/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
				fpprev = *(stack_frame **)(fp + 1);
				continue;
			} else
#			endif
				break;			/* Some other base frame that stops us */
		}
		count++;
	}
	return (count);
}
