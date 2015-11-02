/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "get_ret_targ.h"

GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*msp, *stackbase, *stacktop;

/* return the target of the return for this frame; return NULL if not called as extrinsic */
mval *get_ret_targ(stack_frame **retsf)
{
	stack_frame *sf;

	assert((stackbase >= msp) && (stacktop < msp));
	assert(((stack_frame *)stackbase >= frame_pointer) && ((stack_frame *)stacktop < frame_pointer));

	for (sf = frame_pointer; NULL != sf; sf = sf->old_frame_pointer)
		if (SFT_COUNT & sf->type)	/* a counted frame; look no further */
		{
			if (NULL != retsf)
				*retsf = sf;
			return sf->ret_value;
		}
	return NULL;
}
