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
#include "error.h"
#include "gtmci.h"
#include "op.h"

GBLREF	stack_frame 	*frame_pointer;
GBLREF unsigned char	*msp;

/* routine executed from CI frame (SFF_CI) of the current gtm environment */
void 	ci_ret_code(void)
{
	longjmp(active_ch->jmp, -1);
}

/* routine executed from the bottom-most frame (base frame) of the
   current gtm environment - called when M routine does zgoto 0 */
void	ci_ret_code_exit(void)
{
	op_unwind();
	/* restore frame_pointer stored at msp (see base_frame.c) */
	frame_pointer = *(stack_frame**)msp;
	msp += sizeof(frame_pointer);
	assert(active_ch->ch == &mdb_condition_handler);
	longjmp(active_ch->jmp, -1);
}
