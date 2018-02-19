/****************************************************************
 *								*
 * Copyright (c) 2001-2011 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "error.h"
#include "gtmci.h"
#include "op.h"
#include "error_trap.h"

GBLREF stack_frame 	*frame_pointer;
GBLREF unsigned char	*msp;
GBLREF int		mumps_status;

/* Exit from the current Call-in environment */
void ci_ret_code_quit(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(frame_pointer->type & SFT_CI);
	op_unwind(); 		/* Unwind base frame of this call-in environment */
	gtmci_isv_restore();	/* Restore $ECODE/$STACK of previous level in the nested call-ins. Note MUST be called AFTER
				 * the base CI frame is unwound as the unwind of the MV_STCK mv_stent prepares buffers for
				 * this routine to restore.
				 */
	(TREF(gtmci_nested_level))--;
	/* Restore frame_pointer stored at msp (see base_frame.c) */
	frame_pointer = *(stack_frame**)msp;
	msp += SIZEOF(frame_pointer);
}
