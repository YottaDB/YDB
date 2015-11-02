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

#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "op.h"
#include "dollar_zlevel.h"
#include "golevel.h"
#include "error.h"
#include "gtm_trigger_trc.h"
#include "gtmimagename.h"
#include "send_msg.h"
#include "getzposition.h"

GBLREF	stack_frame	*frame_pointer;
#ifdef GTM_TRIGGER
GBLREF	boolean_t	goframes_unwound_trigger;
#endif

LITREF	gtmImageName 	gtmImageNames[];

void op_zg1(int4 level)
{
	mval	zposition;

	error_def(ERR_PROCTERM);

	/* For ZGOTO 0, if we are running in GTM's runtime (via mumps executable), we allow this to proceed with the
	 * unwind and return back to the caller. However if this is MUPIP, we will exit after sending an oplog message
	 * recording the uncommon exit method.
	 */
	if (0 == level && !IS_GTM_IMAGE)
	{
		zposition.mvtype = 0;	/* It's not an mval yet till getzposition fills it in */
		getzposition(&zposition);
		assert(MV_IS_STRING(&zposition) && (0 < zposition.str.len));
		send_msg(VARLSTCNT(8) ERR_PROCTERM, 6, GTMIMAGENAMETXT(image_type), RTS_ERROR_TEXT("ZGOTO 0"),
			 zposition.str.len, zposition.str.addr);
		exit(ERR_PROCTERM);
	}

	GOLEVEL(level, FALSE);		/* We allow return to a trigger base frame with this flavor ZGOTO */
	assert(level == dollar_zlevel());
	DBGTRIGR((stderr, "op_zg1: Resuming at frame 0x%016lx with type 0x%04lx\n", frame_pointer, frame_pointer->type));
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
