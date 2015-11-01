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
#include "mprof.h"

GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*msp, *stackbase, *stackwarn, *stacktop;

void exfun_frame (void)
{
	register stack_frame *sf;
	unsigned char	*msp_save;
	error_def	(ERR_STACKOFLOW);
	error_def	(ERR_STACKCRIT);

	msp_save = msp;
	sf = (stack_frame *) (msp -= sizeof (stack_frame));
   	if (msp <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp = msp_save;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
		} else
			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	}
	assert (msp < stackbase);
	*sf = *frame_pointer;
	msp -= sf->rvector->temp_size;
   	if (msp <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp = msp;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
		} else
			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	}
	sf->temps_ptr = msp;
	assert (msp < stackbase);
	memset (msp, 0, sf->rvector->temp_size);
	sf->old_frame_pointer = frame_pointer;
	frame_pointer = sf;
	return;
}

void exfun_frame_sp(void)
{
	exfun_frame();
	new_prof_frame (TRUE);
}

void exfun_frame_push_dummy_frame(void)
{
	exfun_frame();
	new_prof_frame (FALSE);
}
