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
#include "op.h"
#include "fgncal.h"

GBLDEF unsigned char *fgncal_stack;
GBLREF unsigned char *stackbase, *stacktop, *stackwarn, *msp;
GBLREF mv_stent *mv_chain;
GBLREF stack_frame *frame_pointer;

void fgncal_unwind(void)
{
	mv_stent *mvc;
	error_def(ERR_STACKUNDERFLO);

	assert(msp <= stackbase && msp > stacktop);
	assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
	assert(frame_pointer <= (stack_frame*)stackbase && frame_pointer > (stack_frame *)stacktop);

	while (frame_pointer && frame_pointer < (stack_frame *)fgncal_stack)
		op_unwind();

	for (mvc = mv_chain ; mvc < (mv_stent *) fgncal_stack ; )
	{
		unw_mv_ent(mvc);
		mvc = (mv_stent *)(mvc->mv_st_next + (char *) mvc);
	}
	mv_chain = mvc;
	msp = fgncal_stack;
	if (msp > stackbase)
		rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);

}
