/****************************************************************
 *								*
 *	Copyright 2002, 2011 Fidelity Information Services, Inc	*
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

GBLREF stack_frame	*frame_pointer;

void adjust_frames(unsigned char *old_ptext_beg, unsigned char *old_ptext_end, unsigned char *new_ptext_beg)
{
	stack_frame	*fp;

	for (fp = frame_pointer; NULL != fp; fp = fp->old_frame_pointer)
	{
#ifdef		GTM_TRIGGER
		if (fp->type & SFT_TRIGR)
			/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
			fp = *(stack_frame **)(fp + 1);
		assert(fp);
#endif
		assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
		if (old_ptext_beg <= fp->mpc && fp->mpc <= old_ptext_end)
			fp->mpc += (new_ptext_beg - old_ptext_beg);
	}
	return;
}
