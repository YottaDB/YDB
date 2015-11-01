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
#include "cache.h"
#include "objlabel.h"
#include "op.h"

GBLREF void		(*unw_prof_frame_ptr)(void);
GBLREF stack_frame	*frame_pointer, *zyerr_frame;
GBLREF unsigned char	*msp, *stackbase, *stacktop;
GBLREF mv_stent		*mv_chain;
GBLREF tp_frame		*tp_pointer;
GBLREF boolean_t	is_tracing_on;

void op_unwind(void)
{
	mv_stent 	*mvc;

	error_def(ERR_STACKUNDERFLO);
	error_def(ERR_TPQUIT);

	if (tp_pointer && tp_pointer->fp <= frame_pointer)
		rts_error(VARLSTCNT(1) ERR_TPQUIT);
	assert(msp <= stackbase && msp > stacktop);
	assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
	assert(frame_pointer <= (stack_frame*)stackbase && frame_pointer > (stack_frame *)stacktop);

	/* See if unwinding an indirect frame*/
	IF_INDR_FRAME_CLEANUP_CACHE_ENTRY(frame_pointer);

	for (mvc = mv_chain; mvc < (mv_stent *)frame_pointer; )
	{
		unw_mv_ent(mvc);
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
	}
	if (is_tracing_on)
		(*unw_prof_frame_ptr)();
	mv_chain = mvc;
	msp = (unsigned char *)frame_pointer + sizeof(stack_frame);
	if (msp > stackbase)
		rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
	frame_pointer = frame_pointer->old_frame_pointer;
	if (NULL != zyerr_frame && frame_pointer > zyerr_frame)
		zyerr_frame = NULL;
	if (frame_pointer)
	{
		if (frame_pointer < (stack_frame *)msp || frame_pointer > (stack_frame *)stackbase ||
				frame_pointer < (stack_frame *)stacktop)
			rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
	}
	return;
}
