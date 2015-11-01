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
#include "io.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "error.h"
#include "gtmci.h"
#include "op.h"
#include "arg_save_area.h"

GBLREF	stack_frame 	*frame_pointer;
GBLREF 	unsigned char	*msp;
GBLREF  int     	mumps_status, dollar_truth;

void 	ci_start(void)
{
	/* extend stack to be used as argument save area.
	   all M routines execute in this super stack frame */
	char	arg_save_area[ARG_SAVE_AREA_SIZE];

	dollar_truth = mumps_status = 1;
	assert(frame_pointer->flags & SFF_CI);
        frame_pointer->ctxt = CONTEXT(ci_restart);
	frame_pointer->mpc = CODE_ADDRESS(ci_restart);

	ESTABLISH(mdb_condition_handler);

	/* 'arg_save_area' passed to make sure compiler doesn't remove
	 * this redundant local array */
	(*restart)(arg_save_area);

	GTMASSERT;
}
