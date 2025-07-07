/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "zr_unlink_rtn.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#endif
#include "xfer_enum.h"
#include "have_crit.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"
#include "ztimeout_routines.h"
#include "jobinterrupt_process.h"
#include "try_event_pop.h"

GBLREF	boolean_t	dollar_truth, is_tracing_on, skip_error_ret;
GBLREF	mval		*alias_retarg;
GBLREF	mv_stent	*mv_chain;
GBLREF	stack_frame	*error_frame, *frame_pointer, *zyerr_frame;
GBLREF	tp_frame	*tp_pointer;
GBLREF	unsigned char	*msp, *stackbase, *stacktop;
GBLREF	void		(*unw_prof_frame_ptr)(void);
GBLREF	volatile int4	outofband;
GBLREF symval			*curr_symval;

error_def(ERR_STACKUNDERFLO);
error_def(ERR_TPQUIT);

/* This has to be maintained in parallel with unw_retarg(), the unwind with a return argument (extrinisic quit) routine. */
void op_unwind(void)
{
	boolean_t	will_underflow;
	mv_stent	*mvc, *mv_prev, *mv_curr;
	unsigned int	lcl_type, lcl_diff, lcl_size;
	unsigned char	*lcl_msp, *lcl_mv_chain;
	rhdtyp		*rtnhdr;
	stack_frame 	*prevfp;
	unsigned short	prevtype;
	boolean_t	unwound_stent;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	if (frame_pointer->type & SFT_COUNT)
	{	/* If unwinding a counted frame, make sure we clean up any alias return argument in flight */
		if (NULL != alias_retarg)
			CLEAR_ALIAS_RETARG;
	}
	prevfp = frame_pointer;
	/* Once we pop off this frame we can overwrite this with moved new'd vars, so store for assert */
	prevtype = prevfp ? prevfp->type : 0;
	if (tp_pointer && tp_pointer->fp <= frame_pointer)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TPQUIT);
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
	mv_prev = NULL;
	will_underflow = (stackbase < ((unsigned char *)frame_pointer + SIZEOF(stack_frame)));
	will_underflow = will_underflow || (frame_pointer->old_frame_pointer
			&& ((frame_pointer->old_frame_pointer < (stack_frame *)((char *)frame_pointer + SIZEOF(stack_frame)))
				|| (frame_pointer->old_frame_pointer > (stack_frame *)stackbase)
				|| (frame_pointer->old_frame_pointer < (stack_frame *)stacktop)));
	lcl_diff = ((unsigned char *)mv_chain - stacktop);
	for (mvc = mv_chain; mvc < (mv_stent *)frame_pointer; )
	{
		unwound_stent = unw_mv_ent(mvc,
				((will_underflow || (SFT_COUNT & frame_pointer->type)) ? UNWIND_NEWVARS : RETAIN_NEWVARS));
		mv_curr = mvc;
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
		if (!unwound_stent)
		{
			assert((!will_underflow && !(SFT_COUNT & frame_pointer->type) && ((MVST_MSAV == mv_curr->mv_st_type)
				|| (MVST_NVAL == mv_curr->mv_st_type) || (MVST_STAB == mv_curr->mv_st_type)
				|| (MVST_L_SYMTAB == mv_curr->mv_st_type))));
			/* Create backwards list of stack elements to push */
			mv_curr->mv_st_next = (mv_prev) ? ((char *)mv_curr - (char *)mv_prev) : 0;
			mv_prev = mv_curr;
		}
	}
	if (0 <= frame_pointer->dollar_test)		/* get dollar_test if it has been set */
		dollar_truth = frame_pointer->dollar_test;
	if (is_tracing_on GTMTRIG_ONLY( && !(frame_pointer->type & SFT_TRIGR)))
		(*unw_prof_frame_ptr)();
	mv_chain = mvc;
	msp = (unsigned char *)frame_pointer + SIZEOF(stack_frame);
	if (msp > stackbase)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_STACKUNDERFLO);
	if (SSF_NORET_VIA_MUMTSTART & frame_pointer->type)
		DBGTRIGR((stderr, "op_unwind: Unwinding frame 0x"lvaddr" with type %d which has SSF_NORET_VIA_MUMTSTART enabled\n",
			  frame_pointer, frame_pointer->type));
	DRAIN_GLVN_POOL_IF_NEEDED;
	PARM_ACT_UNSTACK_IF_NEEDED;
	USHBIN_ONLY(rtnhdr = frame_pointer->rvector);	/* Save rtnhdr for cleanup call below */
	frame_pointer = frame_pointer->old_frame_pointer;
	DBGEHND((stderr, "op_unwind: Stack frame 0x"lvaddr" unwound - frame 0x"lvaddr" now current - New msp: 0x"lvaddr"\n",
		 prevfp, frame_pointer, msp));
	if (NULL != zyerr_frame && frame_pointer > zyerr_frame)
		zyerr_frame = NULL;	/* If we have unwound past zyerr_frame, clear it */
	if (frame_pointer)
	{
		if ((frame_pointer < (stack_frame *)msp)
				|| (frame_pointer > (stack_frame *)stackbase) || (frame_pointer < (stack_frame *)stacktop))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKUNDERFLO);
		assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	}
	assert(!will_underflow);
	lcl_msp = msp;
	lcl_mv_chain = (unsigned char *)mv_chain;
	for (mv_curr = mv_prev, mv_prev = NULL; mv_curr != mv_prev;
			mv_prev = mv_curr, mv_curr = (mv_stent *)((char *)mv_curr - mv_curr->mv_st_next))
	{
		assert((lcl_msp - mvs_size[mv_curr->mv_st_type]) >= ((unsigned char *)mv_curr + mvs_size[mv_curr->mv_st_type]));
		assert(prevfp && !(SFT_COUNT & prevtype));
		lcl_type = mv_curr->mv_st_type;
		lcl_size = mvs_size[lcl_type];
		lcl_msp -= lcl_size;
		memcpy(lcl_msp, mv_curr, lcl_size);
		lcl_diff = (lcl_mv_chain - lcl_msp);
		assert(((mv_stent *)lcl_msp)->mv_st_type == lcl_type);
		((mv_stent *)lcl_msp)->mv_st_next = lcl_diff;
		lcl_mv_chain = lcl_msp;
	}
	msp = lcl_msp;
	mv_chain = (mv_stent *)lcl_mv_chain;
	USHBIN_ONLY(CLEANUP_COPIED_RECURSIVE_RTN(rtnhdr));
	/* We just unwound a frame. May have been either a zintrupt frame and/or may have unwound a NEW'd ZTRAP or even cleared
	 * our error state. If we have a deferred timeout and none of the deferral conditions are anymore in effect, release
	 * the hounds.
	 */
	TRY_EVENT_POP;
	return;
}
