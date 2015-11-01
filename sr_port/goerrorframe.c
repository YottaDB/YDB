/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

GBLREF	stack_frame	*frame_pointer;

void	goerrorframe(stack_frame *stop_fp)
{
        stack_frame     *fp;
        int4            unwind;

        for (unwind = 0, fp = frame_pointer; fp < stop_fp; fp = fp->old_frame_pointer)
		unwind++;
	assert(fp == stop_fp);
	goframes(unwind);
        return;
}
