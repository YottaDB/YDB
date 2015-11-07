/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "tp_frame.h"
#include "golevel.h"
#include "error.h"
#include "dollar_zlevel.h"

GBLREF	stack_frame	*frame_pointer;

error_def(ERR_ZGOTOTOOBIG);
error_def(ERR_ZGOTOLTZERO);

void	golevel(int4 level)
{
        stack_frame     *fp;
        int4            unwind, zlevel, prevlvl;

	if (0 > level)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOTOLTZERO);
	for (zlevel = 0, fp = frame_pointer; fp->old_frame_pointer; fp = fp->old_frame_pointer)
	{
		if (fp->type & SFT_COUNT)
			zlevel++;
	}
	if (level > zlevel)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOTOTOOBIG);
	for (unwind = 0, fp = frame_pointer; fp->old_frame_pointer; fp = fp->old_frame_pointer)
	{
		if ((fp->type & SFT_COUNT) && (level == zlevel--))
			break;
		unwind++;
	}
	DBGEHND_ONLY(prevlvl = dollar_zlevel());
	GOFRAMES(unwind, FALSE, FALSE);
	DBGEHND((stderr, "golevel: Unwound from level %d to level %d  which is %d frames ending in stackframe 0x"lvaddr" with"
		 " type 0x%04lx\n", prevlvl, level, unwind, frame_pointer, (frame_pointer ? frame_pointer->type : 0xffff)));
	return;
}
