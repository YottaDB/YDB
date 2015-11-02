/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"

#include "error.h"
#include "error_trap.h"
#include "dollar_zlevel.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "golevel.h"

#ifdef UNIX
#define BASE_FRAME(fp) ((fp->type & SFT_DM) || (fp->flags & SFF_CI))
#elif defined(VMS)
#define BASE_FRAME(fp) ((fp->type & SFT_DM))
#endif

GBLREF	stack_frame		*frame_pointer;
GBLREF	unsigned short		proc_act_type;
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	int			mumps_status;
GBLREF	stack_frame		*error_frame;

void error_return(void)
{
	int		parent_level;
	stack_frame	*curframe, *cur_counted_frame, *parent_counted_frame;
	boolean_t	rethrow_needed = FALSE, dev_act_err;

	error_def(ERR_REPEATERROR);

	assert((frame_pointer->flags & SFF_ETRAP_ERR) || (error_frame == frame_pointer));
	/* Determine counted frame at the current $zlevel */
	for (curframe = frame_pointer; (NULL != curframe) && !(curframe->type & SFT_COUNT) && !BASE_FRAME(curframe);
			curframe = curframe->old_frame_pointer)
		;
	cur_counted_frame = curframe;
	assert((NULL != error_frame) || ((NULL != cur_counted_frame) && (cur_counted_frame->flags & SFF_ETRAP_ERR)));
	assert((NULL == error_frame) || ((NULL != cur_counted_frame) && !(cur_counted_frame->flags & SFF_DEV_ACT_ERR)));
	NULLIFY_ERROR_FRAME;	/* reset error_frame */
	dev_act_err = ((NULL != cur_counted_frame) && (cur_counted_frame->flags & SFF_ETRAP_ERR)
		&& (cur_counted_frame->flags & SFF_DEV_ACT_ERR));
	/* If the top level error is a device exception error, we do not want to unwind upto the parent frame but instead
	 * rethrow the error at the current level and use $ztrap/$etrap exception handling. In case even that fails,
	 * we will come again to error_return at which point, we will do the unwind upto the parent frame.
	 */
	if (!dev_act_err)
	{	/* Determine counted frame at the parent  $zlevel */
		if (NULL != curframe)
		{
			for (curframe = curframe->old_frame_pointer;
					(NULL != curframe) && !(curframe->type & SFT_COUNT) && !BASE_FRAME(curframe);
					curframe = curframe->old_frame_pointer)
				;
		}
	}
	parent_counted_frame = curframe;
	/* Exit if we are at the bottom of the stack */
	parent_level = dollar_zlevel() - 1;
	if ((NULL == parent_counted_frame) || (parent_level < 1))
		EXIT(dollar_ecode.error_last_ecode);
	assert(parent_level > 0);
	if (dev_act_err || (!BASE_FRAME(parent_counted_frame) && dollar_ecode.index))
		rethrow_needed = TRUE;
	if (!dev_act_err)
	{
		if (parent_counted_frame->type & SFT_DM)
		{	/* hack to retain SFT_DM frame from being unwound by golevel */
			parent_counted_frame->type |= SFT_COUNT;
			golevel(parent_level+1);
			parent_counted_frame->type &= ~SFT_COUNT;
			assert(parent_counted_frame->type & SFT_DM);
		} else
			golevel(parent_level);
		/* Check that we have unwound exactly upto the parent counted frame. */
		assert(parent_counted_frame == frame_pointer);
	} else
	{
		golevel(parent_level + 1);
		/* Check that we have unwound exactly upto the current counted frame. */
		assert(cur_counted_frame == frame_pointer);
	}
	assert(!proc_act_type);
	if (rethrow_needed)
	{
		rts_error(VARLSTCNT(1) ERR_REPEATERROR);
		assert(FALSE);	/* the previous rts_error() should not return */
	}
	UNIX_ONLY(
		if (parent_counted_frame->flags & SFF_CI) /* Unhandled error in call-in: return to gtm_ci */
			mumps_status = dollar_ecode.error_last_ecode;
		MUM_TSTART;
	)
}
