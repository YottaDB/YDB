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
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;

void new_stack_frame(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr)
{
	register stack_frame *sf;
	unsigned char	*msp_save;
	short int	x1, x2;
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	msp_save = msp;
	sf = (stack_frame *) (msp -= sizeof(stack_frame));
   	if (msp <= stackwarn)
   	{
		if (msp <= stacktop)
   		{
			msp = msp_save;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
   		} else
			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
   	}
	assert((unsigned char *) msp < stackbase);
	sf->old_frame_pointer = frame_pointer;
	sf->rvector = rtn_base;
	sf->vartab_ptr = (char *) rtn_base + sf->rvector->vartab_ptr;
	sf->vartab_len = sf->rvector->vartab_len;
	sf->ctxt = context;
	sf->mpc = transfer_addr;
	sf->flags = 0;
#if defined(__alpha) || defined(__MVS__) || defined(__s390__)
	sf->literal_ptr = (int4 *)0;
#endif
	sf->temp_mvals = sf->rvector->temp_mvals;
	msp -= x1 = rtn_base->temp_size;
	sf->temps_ptr = msp;
	sf->type = SFT_COUNT;
	msp -= x2 = rtn_base->vartab_len * sizeof(mval*);
	sf->l_symtab = (mval **) msp;
   	if (msp <= stackwarn)
   	{
		if (msp <= stacktop)
   		{
			msp = msp_save;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
   		} else
			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
   	}
	assert(msp < stackbase);
	memset(msp, 0, x1 + x2);
	frame_pointer = sf;
	return;
}

void new_stack_frame_sp(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr)
{
	new_stack_frame(rtn_base, context, transfer_addr);
	new_prof_frame(TRUE);
}
