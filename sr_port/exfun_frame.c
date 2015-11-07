/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mprof.h"
#include "error.h"
#include "glvn_pool.h"

GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*msp, *stackbase, *stackwarn, *stacktop;

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void exfun_frame (void)
{
	register stack_frame	*sf;
	unsigned char		*msp_save;

	msp_save = msp;
	sf = (stack_frame *)(msp -= SIZEOF(stack_frame));	/* Note imbedded assignment */
	assert(sf < frame_pointer);
	if (msp <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp = msp_save;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKOFLOW);
		} else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKCRIT);
	}
	assert (msp < stackbase);
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	*sf = *frame_pointer;
	msp -= sf->rvector->temp_size;
	if (msp <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp = msp_save;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKOFLOW);
		} else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKCRIT);
	}
	sf->temps_ptr = msp;
	assert(msp < stackbase);
	memset(msp, 0, sf->rvector->temp_size);
	SET_GLVN_INDX(sf, GLVN_POOL_UNTOUCHED);
	sf->ret_value = NULL;
	sf->dollar_test = -1;
	sf->old_frame_pointer = frame_pointer;
	sf->flags = 0;			/* Don't propagate special flags */
	sf->type &= SFT_ZINTR_OFF;	/* Don't propagate special type - normally can't propagate but if $ZINTERRUPT frame is
					 * rewritten by ZGOTO to a "regular" frame, this frame type *can* propagate.
					 */
	frame_pointer = sf;
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	DBGEHND((stderr, "exfun_frame: Added stackframe at addr 0x"lvaddr"  old-msp: 0x"lvaddr"  new-msp: 0x"lvaddr" for routine "
		 "%.*s (rtnhdr 0x"lvaddr")\n", sf, msp_save, msp, sf->rvector->routine_name.len, sf->rvector->routine_name.addr,
		 sf->rvector));
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
