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

#include "gtm_stdio.h"
#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mprof.h"
#include "error.h"
#include "glvn_pool.h"

GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void new_stack_frame(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr)
{
	register stack_frame 	*sf;
	unsigned char		*msp_save;
	unsigned int		x1, x2;

	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	msp_save = msp;
	sf = (stack_frame *)(msp -= SIZEOF(stack_frame));
	if (msp <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp = msp_save;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
		} else
			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	}
	assert((unsigned char *)msp < stackbase);
	sf->old_frame_pointer = frame_pointer;
	sf->rvector = rtn_base;
	sf->vartab_ptr = (char *)VARTAB_ADR(rtn_base);
	sf->vartab_len = sf->rvector->vartab_len;
	sf->ctxt = context;
	sf->mpc = transfer_addr;
	sf->flags = 0;
	SET_GLVN_INDX(sf, GLVN_POOL_UNTOUCHED);
	sf->ret_value = NULL;
	sf->dollar_test = -1;
#ifdef HAS_LITERAL_SECT
	sf->literal_ptr = (int4 *)LITERAL_ADR(rtn_base);
#endif
	sf->temp_mvals = sf->rvector->temp_mvals;
	msp -= x1 = rtn_base->temp_size;
	sf->temps_ptr = msp;
	sf->type = SFT_COUNT;
	msp -= x2 = rtn_base->vartab_len * SIZEOF(ht_ent_mname *);
	sf->l_symtab = (ht_ent_mname **)msp;
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
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	DBGEHND((stderr, "new_stack_frame: Added stackframe at addr 0x"lvaddr"  old-msp: 0x"lvaddr"  new-msp: 0x"lvaddr"\n",
		 sf, msp_save, msp));
	return;
}

void new_stack_frame_sp(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr)
{
	new_stack_frame(rtn_base, context, transfer_addr);
	new_prof_frame(TRUE);
}
