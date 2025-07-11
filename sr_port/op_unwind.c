/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

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
#include "gtmio.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#endif
#include "xfer_enum.h"
#include "have_crit.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"
#include "ztimeout_routines.h"
#include "jobinterrupt_process.h"
#include "bool_zysqlnull.h"
#include "try_event_pop.h"

GBLREF	boolean_t	dollar_truth, is_tracing_on, skip_error_ret;
GBLREF	mval		*alias_retarg;
GBLREF	mv_stent	*mv_chain;
GBLREF	stack_frame	*error_frame, *frame_pointer, *zyerr_frame;
GBLREF	tp_frame	*tp_pointer;
GBLREF	unsigned char	*msp, *stackbase, *stacktop;
GBLREF	void		(*unw_prof_frame_ptr)(void);
GBLREF	volatile int4	outofband;

error_def(ERR_STACKUNDERFLO);
error_def(ERR_TPQUIT);

/* This has to be maintained in parallel with unw_retarg(), the unwind with a return argument (extrinisic quit) routine. */
void op_unwind(void)
{
	boolean_t	defer_tptimeout, defer_ztimeout;
	mv_stent	*mvc;
	rhdtyp		*rtnhdr;
	boolean_t	trig_forced_unwind;
	DBGEHND_ONLY(stack_frame *prevfp;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	trig_forced_unwind = TREF(trig_forced_unwind); 	/* save a copy of global into local */
	TREF(trig_forced_unwind) = FALSE;	/* reset this global right away in case we hit an error codepath below */
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	if (frame_pointer->type & SFT_COUNT)
	{	/* If unwinding a counted frame, make sure we clean up any alias return argument in flight */
		if (NULL != alias_retarg)
			CLEAR_ALIAS_RETARG;
	}
	if (gv_namenaked_state == NAMENAKED_ININTERRUPT) {
		/* We have finished executing a stack frame inside an interrupt. But we may not be returning to normal code, because
		 * interrupts can nest. First verify that this is the outermost interrupt. */
		boolean_t frame_is_interrupt = frame_pointer->type & (SFT_ZSTEP_ACT | SFT_ZBRK_ACT | SFT_ZTRAP | SFT_DEV_ACT);
		boolean_t nested_interrupt = false;
		stack_frame *current_frame = frame_pointer;
		while (!nested_interrupt && NULL != current_frame->old_frame_pointer) {
			current_frame = current_frame->old_frame_pointer;
			nested_interrupt |= current_frame->type & (SFT_ZSTEP_ACT | SFT_ZBRK_ACT | SFT_ZTRAP | SFT_DEV_ACT);
		}
		if (frame_is_interrupt && !nested_interrupt)
			/* At this point we know at compile time the code that is executing. But we do not know the state of $REFERENCE.
			* Refrain from optimizing until we see a GVNAME that resets it to a known state.
			*/
			gv_namenaked_state = NAMENAKED_UNKNOWNREFERENCE;
	}
	DBGEHND_ONLY(prevfp = frame_pointer);
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
	TREF(trig_forced_unwind) = trig_forced_unwind;	/* copy local back to global so "unw_mv_ent" can use that.
							 * It is safe to do so as long as "unw_mv_ent" has no error codepath
							 * which is true at this time.
							 */
	for (mvc = mv_chain; mvc < (mv_stent *)frame_pointer; )
	{
		unw_mv_ent(mvc);
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
	}
	TREF(trig_forced_unwind) = FALSE;	/* Again, turn this global flag off while it is not needed in case of error below.
						 * Note that even though "unw_mv_ent" could clear this global in case it unwound
						 * a MVST_TRGR mv_stent, we are not guaranteed an MVST_TRGR mv_stent is there on
						 * the M-stack in all calls to "op_unwind" and hence clear this here too.
						 */
	if (0 <= frame_pointer->dollar_test)		/* get dollar_test if it has been set */
		dollar_truth = frame_pointer->dollar_test;
	if (is_tracing_on GTMTRIG_ONLY( && !(frame_pointer->type & SFT_TRIGR)))
		(*unw_prof_frame_ptr)();
	mv_chain = mvc;
	msp = (unsigned char *)frame_pointer + SIZEOF(stack_frame);
	if (msp > stackbase)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_STACKUNDERFLO);
#	ifdef DEBUG_TRIGR
	if (SFF_NORET_VIA_MUMTSTART & frame_pointer->type)
		DBGTRIGR((stderr, "op_unwind: Unwinding frame 0x"lvaddr" with type %d which has SFF_NORET_VIA_MUMTSTART enabled\n",
			  frame_pointer, frame_pointer->type));
#	endif
	DRAIN_GLVN_POOL_IF_NEEDED;
	PARM_ACT_UNSTACK_IF_NEEDED;
	USHBIN_ONLY(rtnhdr = frame_pointer->rvector);	/* Save rtnhdr for cleanup call below */
	bool_zysqlnull_unwind();
	frame_pointer = frame_pointer->old_frame_pointer;
	DBGEHND((stderr, "op_unwind: Stack frame 0x"lvaddr" unwound - frame 0x"lvaddr" now current - New msp: 0x"lvaddr"\n",
		 prevfp, frame_pointer, msp));
	if (NULL != zyerr_frame && frame_pointer > zyerr_frame)
		zyerr_frame = NULL;	/* If we have unwound past zyerr_frame, clear it */
	if (frame_pointer)
	{
		if ((frame_pointer < (stack_frame *)msp)
				|| (frame_pointer > (stack_frame *)stackbase) || (frame_pointer < (stack_frame *)stacktop))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_STACKUNDERFLO);
		assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	}
	USHBIN_ONLY(CLEANUP_COPIED_RECURSIVE_RTN(rtnhdr));
	/* We just unwound a frame. May have been either a zintrupt frame and/or may have unwound a NEW'd ZTRAP or even cleared
	 * our error state. If we have a deferred timeout and none of the deferral conditions are anymore in effect, release
	 * the hounds.
	 */
	TRY_EVENT_POP;
	return;
}
