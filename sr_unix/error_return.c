/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#include "error.h"
#include "error_trap.h"
#include "dollar_zlevel.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "golevel.h"
#include "io.h"
#include "send_msg.h"

#define BASE_FRAME(fp) ((fp->type & SFT_DM) || (fp->flags & SFF_CI))

GBLREF	stack_frame		*frame_pointer;
GBLREF	unsigned short		proc_act_type;
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	int			mumps_status;
GBLREF	stack_frame		*error_frame;
GBLREF	io_desc			*gtm_err_dev;
GBLREF	mval			dollar_zstatus;

error_def(ERR_JIUNHNDINT);
error_def(ERR_REPEATERROR);

void error_return(void)
{
	int		parent_level, unwcnt;
	stack_frame	*curframe, *cur_counted_frame, *parent_counted_frame;
	boolean_t	rethrow_needed = FALSE, dev_act_err, transcendental, zintrframe;

	DBGEHND((stderr, "error_return: Entered\n"));
	assert((frame_pointer->flags & SFF_ETRAP_ERR) || (error_frame == frame_pointer));
	unwcnt = 0;
	/* Determine counted frame at the current $zlevel */
	for (curframe = frame_pointer;
	     (NULL != curframe) && !(curframe->type & SFT_COUNT) && !BASE_FRAME(curframe);
	     curframe = curframe->old_frame_pointer)
		unwcnt++; /* (don't need to worry about trigger frames here as they are counted and stop the loop) */
	DBGEHND((stderr, "error_return: cur_counted_frame found with unwind count %d\n", unwcnt));
	cur_counted_frame = curframe;
	NULLIFY_ERROR_FRAME;	/* reset error_frame */
	dev_act_err = ((NULL != cur_counted_frame) && (cur_counted_frame->flags & SFF_ETRAP_ERR)
		&& (cur_counted_frame->flags & SFF_DEV_ACT_ERR));
	zintrframe = (SFT_ZINTR & curframe->type);
	if (dev_act_err || (0 != dollar_ecode.index))
		rethrow_needed = TRUE;
	/* If the top level error is a device exception error, we do not want to unwind upto the parent frame but instead
	 * rethrow the error at the current level and use $ztrap/$etrap exception handling. In case even that fails,
	 * we will come again to error_return at which point, we unwind to the parent frame.
	 */
	if (!dev_act_err && NULL != curframe)
	{	/* We need to unwind this frame to either rethrow error or MUM_TSTART. If this lands us on a trigger frame,
		 * whether that is ok or not depends on whether we will end up rethrowing the error or doing a MUM_TSTART. If we
		 * are rethrowing the error, we must not land on a trigger frame but must unroll it too. In the MUM_TSTART case,
		 * we are resuming at that point via a simulated QUIT so this is ok.
		 */
		do
		{
			curframe = curframe->old_frame_pointer;
			unwcnt++;
#			ifdef GTM_TRIGGER
			if (rethrow_needed && (SFT_TRIGR & curframe->type))
			{	/* With a rethrow, we cannot use a trigger base-frame - back up one frame prior to the frame */
				curframe = *(stack_frame **)(curframe + 1);
				unwcnt++;
			}
#			endif
			transcendental = ((SFT_ZTRAP & curframe->type) || (SFT_DEV_ACT & curframe->type));
		} while (transcendental && (NULL != curframe));
	}
	/* Check a couple of conditions that could turn off rethrow_needed:
	 *
	 *   1. If we ended up on a callin frame which needs to just return to the C caller (error handling abandonded but the error
	 *	code is returned to the caller).
	 *   2. If we ended up on a direct mode frame - errors just get printed here.
	 *   3. If we are unwinding a ZINTERRUPT frame (error handling abandoned - send message to operator log).
	 */
	if (rethrow_needed && BASE_FRAME(curframe))
	{	/* If the computed frame is a base frame for call-ins, we cannot do rethrow and must use MUM_TSTART to return to
		 * the call-in invocation code (gtm_ci()).
		 */
		rethrow_needed = FALSE;
		DBGEHND((stderr, "error_return: rethrow_needed set to FALSE due to call-in base frame"));
	}
	if (rethrow_needed && zintrframe)
	{	/* If we are unwinding a ZINTERRUPT frame, error processing stops here but send a message to the operator log
		 * to notify users of the un-handled error.
		 */
		rethrow_needed = FALSE;
		DBGEHND((stderr, "error_return: rethrow_needed set to FALSE due to unwinding errant zinterrupt frame"));
		send_msg(VARLSTCNT(3) ERR_JIUNHNDINT, 2, dollar_zstatus.str.len, dollar_zstatus.str.addr);
	}
	parent_counted_frame = curframe;	/* Not parent frame for device errors -- still at error counted frame */
	/* Exit if we are at the bottom of the stack */
	parent_level = dollar_zlevel() - 1;
	if ((NULL == parent_counted_frame) || (1 > parent_level))
	{
		EXIT(dollar_ecode.error_last_ecode);
	}
	assert(0 < parent_level);
	DBGEHND((stderr, "error_return: unwcnt: %d  rethrow_needed: %d  dev_act_err: %d\n", unwcnt, rethrow_needed, dev_act_err));
	GOFRAMES(unwcnt, FALSE, FALSE);
	assert(parent_counted_frame == frame_pointer);
	assert(!proc_act_type);
	if (rethrow_needed)
	{
		rts_error(VARLSTCNT(1) ERR_REPEATERROR);
		assert(FALSE);	/* the previous rts_error() should not return */
	}
	/* zero the error device just to be safe */
	assert(NULL == gtm_err_dev);
	gtm_err_dev = NULL;
	if (!zintrframe)
	{
		if (parent_counted_frame->flags & SFF_CI)	/* Unhandled error in call-in: return to gtm_ci */
			mumps_status = dollar_ecode.error_last_ecode;
		MUM_TSTART;
	}
	return;							/* Continue in caller (likely getframe macro) */
}
