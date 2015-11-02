/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "op.h"
#include "lv_val.h"
#include "stack_frame.h"

GBLREF	stack_frame		*frame_pointer;

/* free FOR saved indx pointers at the current M stack level */
void op_forfreeindx(void)
{
	assert(NULL != frame_pointer->for_ctrl_stack);
	FREE_SAVED_FOR_INDX(frame_pointer)
	frame_pointer->for_ctrl_stack = NULL;
	return;
}
