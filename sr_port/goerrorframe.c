/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "golevel.h"
#include "error_trap.h"

GBLREF	stack_frame	*frame_pointer;
GBLREF	stack_frame	*error_frame;

void	goerrorframe()
{
        stack_frame     *fp;
        int4            unwind;

        for (unwind = 0, fp = frame_pointer; fp < error_frame; fp = fp->old_frame_pointer)
		unwind++;
	assert(fp == error_frame);
	goframes(unwind);
	assert(error_frame == frame_pointer);
	/* Now that we (the caller mdb_condition_handler) are going to rethrow an error, ensure that the
	 * SFF_ETRAP_ERR bit is set in "error_frame" in case it got reset by flush_jmp.
	 */
	SET_ERROR_FRAME(error_frame);
	assert(error_frame->flags & SFF_ETRAP_ERR);
        return;
}
