/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "dollar_zlevel.h"
#include "golevel.h"
#include "error.h"
#include "gtmimagename.h"
#include "send_msg.h"
#include "getzposition.h"
#include "tp_frame.h"
#include "error_trap.h"
#ifdef GTM_TRIGGER
#include "gtm_trigger_trc.h"
#endif

GBLREF	stack_frame		*frame_pointer;
#ifdef GTM_TRIGGER
GBLREF	boolean_t		goframes_unwound_trigger;
GBLREF	int4			gtm_trigger_depth;
GBLREF	dollar_ecode_type	dollar_ecode;
#endif

LITREF	gtmImageName 		gtmImageNames[];

error_def(ERR_PROCTERM);
error_def(ERR_ZGOTOINVLVL);
error_def(ERR_ZGOTOLTZERO);
error_def(ERR_ZGOTOTOOBIG);

void op_zg1(int4 level)
{
	stack_frame	*fp, *fpprev;
	int4		unwframes, unwlevels, unwtrglvls, curlvl;
	mval		zposition;

	curlvl = dollar_zlevel();
        if (0 > level)
	{	/* Negative level specified, means to use relative level change */
		if ((-level) > curlvl)
			rts_error(VARLSTCNT(1) ERR_ZGOTOLTZERO);
		unwlevels = -level;	/* Level to seek relative to current level */
		level += curlvl;
	} else
	{	/* Else level is the level we wish to achieve - compute unrolls necessary */
		unwlevels = curlvl - level;
		if (0 > unwlevels)
			/* Couldn't get to the level we were trying to unwind to */
			rts_error(VARLSTCNT(1) ERR_ZGOTOTOOBIG);
	}
	/* For ZGOTO 0, if we are running in GTM's runtime (via mumps executable), we allow this to proceed with the
	 * unwind and return back to the caller. However if this is MUPIP, we will exit after sending an oplog message
	 * recording the uncommon exit method.
	 */
	if ((0 == level) && !IS_GTM_IMAGE)
	{
		zposition.mvtype = 0;	/* It's not an mval yet till getzposition fills it in */
		getzposition(&zposition);
		assert(MV_IS_STRING(&zposition) && (0 < zposition.str.len));
		send_msg(VARLSTCNT(9) ERR_PROCTERM, 7, GTMIMAGENAMETXT(image_type), RTS_ERROR_TEXT("ZGOTO 0"),
			 ERR_PROCTERM, zposition.str.len, zposition.str.addr);
		exit(ERR_PROCTERM);
	}
	/* Find the frame we are unwinding to while counting the frames we need to unwind (which we will feed to
	 * GOFRAMES(). As we unwind, keep track of how many trigger base frames we encounter (if triggers are supported)
	 * so we know what the trigger level will be when we do the unwind. This is used for error checking. Note this
	 * routine is very similar to that in golevel() but has the additional trigger level checking we don't want to
	 * put on other callers.
	 */
	unwframes = unwtrglvls = 0;
        for (fp = frame_pointer; NULL != fp; fp = fpprev)
        {
		assert(0 <= unwlevels);
		fpprev = fp->old_frame_pointer;
		if (NULL == fpprev GTMTRIG_ONLY( && !(SFT_TRIGR & fp->type)))
			break;		/* break on base frame not trigger related */
#		ifdef GTM_TRIGGER
		/* Break if level reached -- note trigger base frame is type=counted but is not counted as
		 * part of level. Our unwind level count is not decremented on a trigger base frame.
		 */
		if (SFT_COUNT & fp->type)
		{
			if (0 == unwlevels)
				break;	/* break on reaching target level with a counted frame */
			if (!(SFT_TRIGR & fp->type))
				unwlevels--;
		}
		unwframes++;
		if (SFT_TRIGR & fp->type)
		{	/* Unwinding a trigger base frame leaves a null frame_pointer allowing us to jump over the
			 * base frame to the stack frame beneath..
			 */
			fpprev = *(stack_frame **)(fp + 1);
			assert(NULL != fpprev);
			unwtrglvls++;	/* Count trigger frames unwound */
		}
#		else
		if ((SFT_COUNT & fp->type) && (0 == unwlevels--))
			break;		/* break on reaching target level with a counted frame */
		unwframes++;
#		endif
        }
#	ifdef GTM_TRIGGER
	/* If we are doing triggers and there is an error ($ECODE is not null), verify we are staying within trigger
	 * context (gtm_trigger_depth > 0 and the stackframe we would unwind to is not the first trigger base frame).
	 * Note bypass this error in the special zgoto 0 case which is effectively a halt.
	 */
	DBGEHND((stderr, "op_zg1: dollar_ecode.index: %d  gtm_trigger_depth: %d  unwtrglvls: %d  fp:0x%016lx\n", dollar_ecode.index,
		 gtm_trigger_depth, unwtrglvls, fp));
	if ((0 != level) && (0 < dollar_ecode.index) && (0 < gtm_trigger_depth)
	    && ((0 >= (gtm_trigger_depth - unwtrglvls)) || ((1 == gtm_trigger_depth) && (SFT_TRIGR & fp->type))))
		rts_error(VARLSTCNT(5) ERR_ZGOTOINVLVL, 3, GTMIMAGENAMETXT(image_type), level);
#	endif
	/* Perform actual unwinding of the frames */
	GOFRAMES(unwframes, FALSE, TRUE);
	DBGEHND((stderr, "op_zg1: Unwound from level %d to level %d  which is %d frames ending in stackframe 0x%016lx with"
		 " type 0x%04lx\n", curlvl, level, unwframes, frame_pointer, (frame_pointer ? frame_pointer->type : 0xffff)));
	assert(level == dollar_zlevel());
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
