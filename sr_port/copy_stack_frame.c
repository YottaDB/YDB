/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mprof.h"
#include "error.h"
#include "glvn_pool.h"

GBLREF stack_frame *frame_pointer;
GBLREF unsigned char *stackbase ,*stacktop, *msp, *stackwarn;

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void copy_stack_frame(void)
{
	register stack_frame	*sf;
	unsigned char		*msp_save;

	msp_save = msp;
	sf = (stack_frame *)(msp -= SIZEOF(stack_frame));
	if (msp <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp = msp_save;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKOFLOW);
		} else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKCRIT);
	}
	assert(msp < stackbase);
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	*sf = *frame_pointer;
	sf->old_frame_pointer = frame_pointer;
	sf->flags = 0;			/* Don't propagate special flags */
	sf->type &= SFT_ZINTR_OFF;	/* Don't propagate special type - normally can't propagate but if $ZINTERRUPT frame is
					 * rewritten by ZGOTO to a "regular" frame, this frame type *can* propagate.
					 */
	SET_GLVN_INDX(sf, GLVN_POOL_UNTOUCHED);
	sf->ret_value = NULL;
	sf->dollar_test = -1;		/* initialize it with -1 for indication of not yet being used */
	frame_pointer = sf;
	DBGEHND((stderr, "copy_stack_frame: Added stackframe at addr 0x"lvaddr"  old-msp: 0x"lvaddr"  new-msp: 0x"lvaddr"\n",
		 sf, msp_save, msp));
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
}

void copy_stack_frame_sp(void)
{
	copy_stack_frame();
	new_prof_frame(TRUE);
}
