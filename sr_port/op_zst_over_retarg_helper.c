/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"

GBLREF unsigned char	*zstep_level;
GBLREF stack_frame	*frame_pointer;

/* This function is written in C since it is easier than writing this in assembly for multiple platforms.
 * The main purpose is to ensure that ZSTEP OVER works correctly in a QUIT @x situation.
 * In this case, a nested uncounted frame is created for the @ usage and a OC_RETARG is done from the nested frame
 * and it also returns from the parent counted frame so if a ZSTEP OVER was in effect in the parent frame, the
 * QUIT from the nested uncounted frame should be treated as if it is a QUIT from the parent frame for ZSTEP OVER purposes.
 * We achieve this by modifying the global "zstep_level" to the uncounted frame (that is the check done by
 * "op_zst_over_retarg" (in op_bkpt.s) to decide whether "op_zstepret" or "op_retarg" needs to be invoked.
 */
void	op_zst_over_retarg_helper(void)
{
	stack_frame	*fp;

	for (fp = frame_pointer; fp ; fp = fp->old_frame_pointer)
	{
		if (fp->type & SFT_COUNT)
			break;
	}
	if ((NULL != fp) && ((unsigned char *)fp->old_frame_pointer == zstep_level))
		zstep_level = (unsigned char *)frame_pointer->old_frame_pointer;
}
