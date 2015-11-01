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
#include "mv_stent.h"
#include "tp_frame.h"
#include "unw_retarg.h"
#include "underr.h"

GBLREF void		(*unw_prof_frame_ptr)(void);
GBLREF stack_frame	*frame_pointer, *zyerr_frame;
GBLREF unsigned char	*msp,*stackbase,*stacktop;
GBLREF mv_stent		*mv_chain;
GBLREF tp_frame		*tp_pointer;
GBLREF boolean_t	is_tracing_on;

int unw_retarg(mval *src)
{
	mval		ret_value, *trg;
	bool		got_ret_target;

	error_def(ERR_STACKUNDERFLO);
	error_def(ERR_NOTEXTRINSIC);
	error_def(ERR_TPQUIT);

	if (tp_pointer && tp_pointer->fp <= frame_pointer)
		rts_error(VARLSTCNT(1) ERR_TPQUIT);
	assert(msp <= stackbase && msp > stacktop);
	assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
	assert(frame_pointer <= (stack_frame *)stackbase && frame_pointer > (stack_frame *)stacktop);
	MV_FORCE_DEFINED(src);
	ret_value = *src;
	got_ret_target = FALSE;
	while (mv_chain < (mv_stent *)frame_pointer)
	{
		msp = (unsigned char *)mv_chain;
		if (trg = unw_mv_ent(mv_chain)) /* CAUTION : Assignment */
		{
			assert(!got_ret_target);
			got_ret_target = TRUE;
			*trg = ret_value;
		}
		POP_MV_STENT();
	}
	if (!got_ret_target)
		rts_error(VARLSTCNT(1) ERR_NOTEXTRINSIC);	/* This routine was not invoked as an extrinsic function */
	if (is_tracing_on)
		(*unw_prof_frame_ptr)();
	msp = (unsigned char *)frame_pointer + sizeof(stack_frame);
	frame_pointer = frame_pointer->old_frame_pointer;
	if (NULL != zyerr_frame && frame_pointer > zyerr_frame)
		zyerr_frame = NULL;
	if (!frame_pointer)
		rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
	assert(frame_pointer >= (stack_frame *)msp);
	trg->mvtype |= MV_RETARG;
	return 0;
}
