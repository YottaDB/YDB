/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
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
#include "op.h"
#include "flush_jmp.h"
#include "dollar_zlevel.h"
#include "golevel.h"
#include "auto_zlink.h"
#include "error.h"
#include "gtm_trigger_trc.h"
#include "gtmimagename.h"

GBLREF	stack_frame	*frame_pointer;
#ifdef GTM_TRIGGER
GBLREF	boolean_t	goframes_unwound_trigger;
#endif

LITREF	gtmImageName 	gtmImageNames[];

void op_zgoto(rhdtyp *rtnhdr, lnr_tabent USHBIN_ONLY(*)*lnrptr, int4 level)
{
	mach_inst	*sav_invok_inst;
	lnr_tabent 	USHBIN_ONLY(*)*lnrptr_ala;	/* Holds auto_zlink() arg for label code offset */
	DEBUG_ONLY(int4	dlevel;)

	error_def(ERR_LABELUNKNOWN);
	error_def(ERR_ZGOTOINVLVL);

	if (0 == level)
	{	/* Ignore entryref if unwinding to return */
		op_zg1(level);
		return;		/* We won't be returning here */
	}
#	ifdef GTM_TRIGGER
	if (!IS_GTM_IMAGE && (1 == level))
		/* In MUPIP (or other utility that gets trigger support in the future), levels 0 and 1 refer to
		 * the pseudo baseframe and initial stack levels which are not rewritable (in the case where an
		 * entry ref was coded) and are not resume-able (if no entry ref were specified) so we cannot
		 * permit ZGOTOs to these levels in a utility. (note level 0 is dealt with by op_zg1).
		 */
		rts_error(VARLSTCNT(5) ERR_ZGOTOINVLVL, 3, GTMIMAGENAMETXT(image_type), level);
#	endif
	sav_invok_inst = (mach_inst *)frame_pointer->mpc;	/* Where the ZGOTO was originally done */
	/* Check if the specified entryref is properly resolved or not. Has to be resolved here before we unwind any stackframes
	 * or it just isn't going to go well since auto_zlink references frame_pointer.
	 */
	if (NULL == rtnhdr)
	{	/* Routine header is not resolved, resolve it with autozlink */
		rtnhdr = auto_zlink(sav_invok_inst, &lnrptr_ala);
		if (NULL == rtnhdr)
			GTMASSERT;			/* Should never return from autozlink unresolved */
		lnrptr = lnrptr_ala;
	}
	if (NULL == lnrptr USHBIN_ONLY( || NULL == *lnrptr))
		rts_error(VARLSTCNT(1) ERR_LABELUNKNOWN);
	/* Now that everything is resolved, drop to the requisite level - may not return if it runs into a trigger base frame */
	GOLEVEL(level, TRUE);	/* Unwind the trigger base frame if we run into one */
	assert(level == (dlevel = dollar_zlevel()));
	/* Convert current stack frame to the frame for the entry ref we want to branch to */
	flush_jmp(rtnhdr, (unsigned char *)LINKAGE_ADR(rtnhdr), LINE_NUMBER_ADDR(rtnhdr, USHBIN_ONLY(*)lnrptr));
	DBGEHND((stderr, "op_zgoto: Resuming at frame 0x%016lx with type 0x%04lx\n", frame_pointer, frame_pointer->type));
#	ifdef GTM_TRIGGER
	if (goframes_unwound_trigger)
	{
		/* If goframes() called by golevel unwound a trigger base frame, we must use MUM_TSTART to unroll the
		 * C stack before invoking the return frame. Otherwise we can just return and avoid the overhead that
		 * MUM_TSTART incurs.
		 */
		MUM_TSTART;
	}
#	endif
}
