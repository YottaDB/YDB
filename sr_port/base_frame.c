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

GBLREF unsigned char 	*stacktop, *stackwarn, *msp;
GBLREF stack_frame	*frame_pointer;

void base_frame(rhdtyp *base_address)
{
	void		gtm_ret_code();	/* This is an external which points to code without an entry mask */
	unsigned char	*msp_save;
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	if ((int4)msp & 1)	/* synchronize mumps stack on even boundary */
		msp--;
	if ((int4)msp & 2)
		msp -= 2;

	msp_save = msp;
	msp -= sizeof(stack_frame) + sizeof (stack_frame *);
   	if (msp <= stackwarn)
   	{
		if (msp <= stacktop)
   		{
			msp = msp_save;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
   		}
   		else
   			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
   	}
	*(stack_frame **)((stack_frame *)msp + 1) = frame_pointer;
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer, 0, sizeof(stack_frame));
	frame_pointer->ctxt = CONTEXT(gtm_ret_code);
	frame_pointer->mpc = CODE_ADDRESS(gtm_ret_code);
	frame_pointer->rvector = base_address;
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer;
	frame_pointer->vartab_len = 0;
	frame_pointer->vartab_ptr = (char *)frame_pointer;
	frame_pointer->type = SFT_COUNT;
}
