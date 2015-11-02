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

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "tp_frame.h"
#include "cache.h"
#include "cache_cleanup.h"
#include "objlabel.h"
#include "op.h"
#include "error_trap.h"
#include "error.h"
#include "tp_timeout.h"
#include "compiler.h"
#include "parm_pool.h"
#include "opcode.h"
#include "glvn_pool.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#endif

GBLREF	void			(*unw_prof_frame_ptr)(void);
GBLREF	stack_frame		*frame_pointer, *zyerr_frame;
GBLREF	unsigned char		*msp, *stackbase, *stacktop;
GBLREF	mv_stent		*mv_chain;
GBLREF	tp_frame		*tp_pointer;
GBLREF	boolean_t		is_tracing_on;
GBLREF	boolean_t		skip_error_ret;
GBLREF	stack_frame		*error_frame;
GBLREF	mval			*alias_retarg;
GBLREF	boolean_t		tp_timeout_deferred;
GBLREF	dollar_ecode_type	dollar_ecode;
GBLREF	boolean_t		ztrap_explicit_null;
GBLREF	mval			dollar_ztrap;
GBLREF	boolean_t		dollar_zininterrupt;
GBLREF	boolean_t		dollar_truth;

error_def(ERR_STACKUNDERFLO);
error_def(ERR_TPQUIT);

/* This has to be maintained in parallel with unw_retarg(), the unwind with a return argument (extrinisic quit) routine. */
void op_unwind(void)
{
	mv_stent 		*mvc;
	DBGEHND_ONLY(stack_frame *prevfp;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
					 * case an error occurs before we return to the caller
					 */
	}
	assert(msp <= stackbase && msp > stacktop);
	assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
	assert(frame_pointer <= (stack_frame*)stackbase && frame_pointer > (stack_frame *)stacktop);
	/* See if unwinding an indirect frame */
	IF_INDR_FRAME_CLEANUP_CACHE_ENTRY(frame_pointer);
	for (mvc = mv_chain; mvc < (mv_stent *)frame_pointer; )
	{
		unw_mv_ent(mvc);
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
	}
	if (0 <= frame_pointer->dollar_test)		/* get dollar_test if it has been set */
		dollar_truth = frame_pointer->dollar_test;
	if (is_tracing_on GTMTRIG_ONLY( && !(frame_pointer->type & SFT_TRIGR)))
		(*unw_prof_frame_ptr)();
	mv_chain = mvc;
	msp = (unsigned char *)frame_pointer + SIZEOF(stack_frame);
	if (msp > stackbase)
		rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
#	ifdef GTM_TRIGGER
	if (SFF_IMPLTSTART_CALLD & frame_pointer->type)
		DBGTRIGR((stderr, "op_unwind: Unwinding frame 0x"lvaddr" with type %d which has SFF_IMPLTSTART_CALLD turned on\n",
			  frame_pointer, frame_pointer->type));
#	endif
	DRAIN_GLVN_POOL_IF_NEEDED;
	PARM_ACT_UNSTACK_IF_NEEDED;
	frame_pointer = frame_pointer->old_frame_pointer;
	DBGEHND((stderr, "op_unwind: Stack frame 0x"lvaddr" unwound - frame 0x"lvaddr" now current - New msp: 0x"lvaddr"\n",
		 prevfp, frame_pointer, msp));
	if (NULL != zyerr_frame && frame_pointer > zyerr_frame)
		zyerr_frame = NULL;	/* If we have unwound past zyerr_frame, clear it */
	if (frame_pointer)
	{
		if ((frame_pointer < (stack_frame *)msp) || (frame_pointer > (stack_frame *)stackbase)
		    || (frame_pointer < (stack_frame *)stacktop))
			rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
		assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	}
	/* We just unwound a frame. May have been either a zintrupt frame and/or may have unwound a NEW'd ZTRAP or even cleared
	 * our error state. If we have a deferred timeout and none of the deferral conditions are anymore in effect, release
	 * the hounds.
	 */
	if (tp_timeout_deferred UNIX_ONLY(&& !dollar_zininterrupt) && ((0 == dollar_ecode.index) || !(ETRAP_IN_EFFECT)))
		tptimeout_set(0);
	return;
}
