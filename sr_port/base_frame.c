/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"	/* For DBGEHND() */
#include <rtnhdr.h>
#include "stack_frame.h"

GBLREF unsigned char 	*stacktop, *stackwarn, *msp;
GBLREF stack_frame	*frame_pointer;

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void base_frame(rhdtyp *base_address)
{
	void		gtm_ret_code();	/* This is an external which points to code without an entry mask */
	unsigned char	*msp_save;
	stack_frame	*fp;

	if ((INTPTR_T)msp & 1)	/* synchronize mumps stack on even boundary */
		msp--;
	if ((INTPTR_T)msp & 2)
		msp -= 2;
#ifdef GTM64
	if ((INTPTR_T)msp & 4)
		msp -= 4;
#endif /* GTM64 */

	msp_save = msp;
	msp -= SIZEOF(stack_frame) + SIZEOF(stack_frame *);
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
	frame_pointer = fp = (stack_frame *)msp;
	memset(fp, 0, SIZEOF(stack_frame));
	fp->ctxt = GTM_CONTEXT(gtm_ret_code);
	fp->mpc = CODE_ADDRESS(gtm_ret_code);
	fp->rvector = base_address;
	fp->temps_ptr = (unsigned char *)fp;
	fp->vartab_len = 0;
	fp->vartab_ptr = (char *)fp;
	fp->type = SFT_COUNT;
	fp->ret_value = NULL;
	fp->dollar_test = -1;
	DBGEHND((stderr, "base_frame: New base frame allocated at 0x"lvaddr"\n", fp));
}
