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

#include <rtnhdr.h>
#include "stack_frame.h"
#include "tp_frame.h"
#include "op.h"
#include "unwind_nocounts.h"
#include "error.h"
#include "error_trap.h"

GBLREF stack_frame	*frame_pointer;
GBLREF stack_frame	*error_frame;
GBLREF tp_frame		*tp_pointer;

error_def(ERR_TPQUIT);

bool unwind_nocounts(void)
{	short		unwind;
	bool		dmode;
	stack_frame	*fp;
	boolean_t	sf_error_ret = FALSE;

	dmode = FALSE;
	unwind = 0;
	/* while unwinding uncounted frames, reset "error_frame" to point to the counted frame in case error_frame happens to be
	 * one of the to-be-unwound uncounted frames so we remember to do error_ret() processing (if applicable)
	 * while "op_unwind()"ing the counted frame.
	 */
	for (fp = frame_pointer; !(fp->type & SFT_COUNT) && fp->old_frame_pointer; fp = fp->old_frame_pointer)
	{
		if (error_frame == fp)
		{
			SET_ERROR_FRAME(fp->old_frame_pointer);
			assert(NULL != error_frame);
		}
		if (fp->type & SFT_DM)
			dmode = TRUE;
		unwind++;
	}
	if (tp_pointer && tp_pointer->fp < fp)
		rts_error(VARLSTCNT(1) ERR_TPQUIT);
	while (unwind-- > 0)
	{
		assert(error_frame != frame_pointer);	/* make sure op_unwind() won't call error_ret() */
		op_unwind();
	}
	return(dmode);
}
