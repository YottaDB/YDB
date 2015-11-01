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

#include "gtm_stdlib.h"

#include "error.h"
#include "error_trap.h"
#include "dollar_zlevel.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "golevel.h"

#ifdef UNIX
#define BASE_FRAME(fp) ((fp->type & SFT_DM) || (fp->flags & SFF_CI))
#elif defined(VMS)
#define BASE_FRAME(fp) ((fp->type & SFT_DM))
#endif

GBLREF	int4		error_last_ecode;
GBLREF	stack_frame	*frame_pointer, *error_frame;
GBLREF	unsigned char	proc_act_type;
GBLREF	unsigned char	*error_last_mpc_err;
GBLREF	unsigned char	*error_last_ctxt_err;
GBLREF	stack_frame	*error_last_frame_err;

void error_return(void)
{
	int		parent_level;
	stack_frame	*parent_frame;
	boolean_t	rethrow_needed = FALSE;
	error_def(ERR_REPEATERROR);

	error_frame = NULL;
	error_last_frame_err = NULL;
	error_last_mpc_err = error_last_ctxt_err = NULL;
	parent_level = dollar_zlevel() - 1;
	for (parent_frame = frame_pointer->old_frame_pointer;
		parent_frame && !(parent_frame->type & SFT_COUNT) && !BASE_FRAME(parent_frame);
		parent_frame = parent_frame->old_frame_pointer)
		;
	if ((!parent_frame) || (parent_level < 1))
	                EXIT(error_last_ecode);
	assert(parent_level > 0);
	if (!BASE_FRAME(parent_frame) && ecode_check())
		rethrow_needed = TRUE;
	if (parent_frame->type & SFT_DM)
	{ /* hack to retain SFT_DM frame from being unwound by golevel */
		parent_frame->type |= SFT_COUNT;
		golevel(parent_level+1);
		parent_frame->type &= ~SFT_COUNT;
		assert(parent_frame->type & SFT_DM);
	} else
		golevel(parent_level);
	assert(!proc_act_type);
	if (rethrow_needed)
		rts_error(VARLSTCNT(1) ERR_REPEATERROR);
	UNIX_ONLY(MUM_TSTART;)
}
