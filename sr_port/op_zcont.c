/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stack_frame.h"
#include "op.h"
#include "tp_frame.h"

GBLREF stack_frame	*frame_pointer;
GBLREF tp_frame		*tp_pointer;

error_def(ERR_TPQUIT);

void	op_zcont(void)
{
	short		unwind;
	stack_frame	*fp;

	unwind = 1;
	for (fp = frame_pointer; fp ; fp = fp->old_frame_pointer, unwind++)
	{	if (fp->type & SFT_COUNT)
		{	return;
		}
		if (fp->type & SFT_DM)
		{	break;
		}
	}
	assert(fp);
	/* "fp" is the Direct Mode frame, and ZCONTINUE resumes the frame under it. A TSTART in Direct Mode makes that frame
	 * the frame of the transaction (see "op_tstart"). If that frame is not a counted frame, it is the frame of a line
	 * typed in Direct Mode, of $ZTRAP code, or of a ZBREAK, ZSTEP or device EXCEPTION action that did a BREAK. The QUIT
	 * of such a frame with the transaction active issues TPQUIT, and "trans_code_cleanup" then rolls back the transaction
	 * so that the frame can be QUIT. So issue TPQUIT for the ZCONTINUE before unwinding anything. The
	 * transaction stays active and Direct Mode stays at the same level, so the user can end the transaction (TCOMMIT or
	 * TROLLBACK) and ZCONTINUE again. A counted frame (e.g. after a BREAK in a routine) resumes a routine, which can end
	 * the transaction before it QUITs, so ZCONTINUE is allowed for it.
	 */
	if ((NULL != tp_pointer) && (NULL != fp->old_frame_pointer) && !(fp->old_frame_pointer->type & SFT_COUNT)
			&& (tp_pointer->fp <= fp->old_frame_pointer))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TPQUIT);
	while (unwind-- > 0)
		op_unwind();
}/* eor */
