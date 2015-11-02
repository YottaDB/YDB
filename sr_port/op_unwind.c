/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "tp_frame.h"
#include "cache.h"
#include "cache_cleanup.h"
#include "objlabel.h"
#include "op.h"
#include "error_trap.h"
#include "error.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#endif

GBLREF	void		(*unw_prof_frame_ptr)(void);
GBLREF	stack_frame	*frame_pointer, *zyerr_frame;
GBLREF	unsigned char	*msp, *stackbase, *stacktop;
GBLREF	mv_stent	*mv_chain;
GBLREF	tp_frame	*tp_pointer;
GBLREF	boolean_t	is_tracing_on;
GBLREF	boolean_t	skip_error_ret;
GBLREF	stack_frame	*error_frame;
GBLREF	mval		*alias_retarg;

error_def(ERR_STACKUNDERFLO);
error_def(ERR_TPQUIT);

/* this has to be maintained in parallel with unw_retarg(), the unwind with a return argument (extrinisic quit) routine */
void op_unwind(void)
{
	mv_stent 	*mvc;
	stack_frame	*rfp;

	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	if (frame_pointer->type & SFT_COUNT)
	{	/* If unwinding a counted frame, make sure we don't have an alias return argument in flight */
		assert(NULL == alias_retarg);
		alias_retarg = NULL;
	}
	DBGEHND_ONLY(prevfp = frame_pointer);
	if (tp_pointer && tp_pointer->fp <= frame_pointer)
		rts_error(VARLSTCNT(1) ERR_TPQUIT);
	/* Note that error_ret() should be invoked only after the rts_error() of TPQUIT.
	 * This is so the TPQUIT error gets noted down in $ECODE (which will not happen if error_ret() is called before).
	 */
	if (!skip_error_ret)
	{
		INVOKE_ERROR_RET_IF_NEEDED;
	} else
	{
		if (NULL != error_frame)
		{
			assert(error_frame >= frame_pointer);
			if (error_frame <= frame_pointer)
				NULLIFY_ERROR_FRAME;	/* ZGOTO to frame level lower than primary error level cancels error mode */
		}
		skip_error_ret = FALSE;	/* reset at the earliest point although caller (goframes()) does reset it just in
					 * case an error occurs before we return to the caller */
	}
	assert(msp <= stackbase && msp > stacktop);
	assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
	assert(frame_pointer <= (stack_frame*)stackbase && frame_pointer > (stack_frame *)stacktop);
	if (NULL != frame_pointer->for_ctrl_stack)
	{	/* someone used an ugly FOR control variable */
		if (frame_pointer->flags & SFF_INDCE)
		{	/* FOR control variable indx in an indirect frame belongs in the underlying real frame */
			for (rfp = frame_pointer->old_frame_pointer; rfp && (rfp->flags & SFF_INDCE); rfp = rfp->old_frame_pointer)
				;
			assert(rfp);
			if (NULL == rfp->for_ctrl_stack)
				rfp->for_ctrl_stack = frame_pointer->for_ctrl_stack;
			else	/* indirect compilation already cloned the pointer */
				assert(rfp->for_ctrl_stack == frame_pointer->for_ctrl_stack);
		} else	/* otherwise, done with this level - clean it up */
			FREE_SAVED_FOR_INDX(frame_pointer);
		frame_pointer->for_ctrl_stack = NULL;
	}
	/* See if unwinding an indirect frame */
	IF_INDR_FRAME_CLEANUP_CACHE_ENTRY(frame_pointer);
	for (mvc = mv_chain; mvc < (mv_stent *)frame_pointer; )
	{
		unw_mv_ent(mvc);
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
	}
	if (is_tracing_on GTMTRIG_ONLY( && !(frame_pointer->type & SFT_TRIGR)))
		(*unw_prof_frame_ptr)();
	mv_chain = mvc;
	msp = (unsigned char *)frame_pointer + SIZEOF(stack_frame);
	if (msp > stackbase)
		rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
#	ifdef GTM_TRIGGER
	if (SFF_TRIGR_CALLD & frame_pointer->type)
		DBGTRIGR((stderr, "op_unwind: Unwinding frame 0x"lvaddr" with type %d which has SFF_TRIGR_CALLD turned on\n",
			  frame_pointer, frame_pointer->type));
#	endif
	frame_pointer = frame_pointer->old_frame_pointer;
	DBGEHND((stderr, "op_unwind: Stack frame 0x"lvaddr" unwound - frame 0x"lvaddr" now current - New msp: 0x"lvaddr"\n",
		 prevfp, frame_pointer, msp));
	if (NULL != zyerr_frame && frame_pointer > zyerr_frame)
		zyerr_frame = NULL;
	if (frame_pointer)
	{
		if (frame_pointer < (stack_frame *)msp || frame_pointer > (stack_frame *)stackbase
			|| frame_pointer < (stack_frame *)stacktop)
				rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
		assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	}
	return;
}
