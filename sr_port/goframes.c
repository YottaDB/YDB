/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "op.h"
#include "get_ret_targ.h"
#include "rtnhdr.h"		/* needed for golevel.h */
#include "stack_frame.h"	/* needed for golevel.h */
#include "tp_frame.h"		/* needed for golevel.h */
#include "golevel.h"

GBLREF	boolean_t	skip_error_ret;
GBLREF	tp_frame	*tp_pointer;
GBLREF	stack_frame	*frame_pointer;

LITREF mval             literal_null;

void	goframes(int4 frames)
{
        mval            *ret_targ;

        for (ret_targ = NULL; frames--; )
        {
		while (tp_pointer && tp_pointer->fp <= frame_pointer)
               	        op_trollback(-1);
		if (0 == frames)
		{
			ret_targ = get_ret_targ();
	       		if (NULL != ret_targ)
	       		{
	       		        *ret_targ = literal_null;
	       		        ret_targ->mvtype |= MV_RETARG;
	       		}
		}
		skip_error_ret = TRUE;
		op_unwind();
		assert(FALSE == skip_error_ret);/* op_unwind() should have read and reset this */
		skip_error_ret = FALSE;		/* be safe in PRO versions */
	}
	return;
}
