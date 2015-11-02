/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "error_trap.h"
#ifdef GTM_TRIGGER
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gv_trigger.h"
#include "gtm_trigger.h"
#endif

GBLREF	stack_frame	*frame_pointer;
GBLREF	stack_frame	*error_frame;

void	goerrorframe()
{
        stack_frame     *fp, *fpprev;
        int4            unwind;

        for (unwind = 0, fp = frame_pointer; fp < error_frame; fp = fpprev)
	{
		fpprev = fp->old_frame_pointer;
#		ifdef GTM_TRIGGER
		if (SFT_TRIGR & fpprev->type)
		{
			fpprev = *(stack_frame **)(fpprev + 1);
			unwind++;	/* Skipping over trigger frame but it needs unwinding too */
		}
#		endif
		unwind++;
		assert(fpprev);
	}
	assert(fp == error_frame);
	DBGEHND((stderr, "goerrorframe: Unwinding %d frames\n", unwind));
	GOFRAMES(unwind, FALSE, FALSE);
	assert(error_frame == frame_pointer);
	/* Now that we (the caller mdb_condition_handler) are going to rethrow an error, ensure that the
	 * SFF_ETRAP_ERR bit is set in "error_frame" in case it got reset by flush_jmp.
	 */
	SET_ERROR_FRAME(error_frame);
	assert(error_frame->flags & SFF_ETRAP_ERR);
        return;
}
