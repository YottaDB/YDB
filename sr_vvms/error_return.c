/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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

#define BASE_FRAME(fp) ((fp->type & SFT_DM))

GBLREF	stack_frame		*frame_pointer;
GBLREF	unsigned short		proc_act_type;
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	int			mumps_status;
GBLREF	stack_frame		*error_frame;
GBLREF	io_desc			*gtm_err_dev;

error_def(ERR_REPEATERROR);

void error_return(void)
{
	int		parent_level;
	stack_frame	*curframe, *cur_counted_frame, *parent_counted_frame;
	boolean_t	rethrow_needed = FALSE, dev_act_err;

	DBGEHND((stderr, "error_return: Entered\n"));
	assert((frame_pointer->flags & SFF_ETRAP_ERR) || (error_frame == frame_pointer));
	/* Determine counted frame at the current $zlevel */
	for (curframe = frame_pointer; (NULL != curframe) && !(curframe->type & SFT_COUNT) && !BASE_FRAME(curframe);
			curframe = curframe->old_frame_pointer)
		;
	cur_counted_frame = curframe;
	NULLIFY_ERROR_FRAME;	/* reset error_frame */
	dev_act_err = ((NULL != cur_counted_frame) && (cur_counted_frame->flags & SFF_ETRAP_ERR)
		&& (cur_counted_frame->flags & SFF_DEV_ACT_ERR));
	/* If the top level error is a device exception error, we do not want to unwind upto the parent frame but instead
	 * rethrow the error at the current level and use $ztrap/$etrap exception handling. In case even that fails,
	 * we will come again to error_return at which point, we will do the unwind upto the parent frame.
	 */
	if (!dev_act_err && (NULL != curframe))		/* Determine counted frame at the parent  $zlevel */
		for (curframe = curframe->old_frame_pointer;
		    (NULL != curframe) && !(curframe->type & SFT_COUNT) && !BASE_FRAME(curframe);
		    curframe = curframe->old_frame_pointer)
			;
	parent_counted_frame = curframe;
	/* Exit if we are at the bottom of the stack */
	parent_level = dollar_zlevel() - 1;
	if ((NULL == parent_counted_frame) || (1 > parent_level))
		EXIT(dollar_ecode.error_last_ecode);
	assert(parent_level > 0);
	if (dev_act_err || (!BASE_FRAME(parent_counted_frame) && dollar_ecode.index))
		rethrow_needed = TRUE;
	DBGEHND((stderr, "error_return: rethrow_needed: %d  dev_act_err: %d\n", rethrow_needed, dev_act_err));
	if (!dev_act_err)
	{
		if (parent_counted_frame->type & SFT_DM)
		{	/* hack to retain SFT_DM frame from being unwound by golevel */
			parent_counted_frame->type |= SFT_COUNT;
			GOLEVEL(parent_level + 1, FALSE);
			parent_counted_frame->type &= ~SFT_COUNT;
			assert(parent_counted_frame->type & SFT_DM);
		} else
			GOLEVEL(parent_level, FALSE);
		/* Check that we have unwound exactly upto the parent counted frame. */
		assert(parent_counted_frame == frame_pointer);
	} else
	{
		GOLEVEL(parent_level + 1, FALSE);
		/* Check that we have unwound exactly upto the current counted frame. */
		assert(cur_counted_frame == frame_pointer);
	}
	assert(!proc_act_type);
	if (rethrow_needed)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REPEATERROR);
		assert(FALSE);	/* the previous rts_error() should not return */
	}
	/* zero the error device just to be safe */
	assert(NULL == gtm_err_dev);
	gtm_err_dev = NULL;
}
