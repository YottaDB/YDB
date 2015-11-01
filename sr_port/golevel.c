/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "tp_frame.h"
#include "op.h"
#include "get_ret_targ.h"
#include "golevel.h"

GBLREF stack_frame      *frame_pointer;
GBLREF tp_frame         *tp_pointer;

LITREF mval             literal_null;

void golevel(int4 level)
{
        stack_frame     *fp;
        int4            unwind, zlevel;
        mval            *ret_targ;

        error_def(ERR_ZGOTOTOOBIG);
        error_def(ERR_ZGOTOLTZERO);

        if (0 > level)
                rts_error(VARLSTCNT(1) ERR_ZGOTOLTZERO);
        for (zlevel = 0, fp = frame_pointer; fp->old_frame_pointer; fp = fp->old_frame_pointer)
        {
                if (fp->type & SFT_COUNT)
                        zlevel++;
        }
        if (level > zlevel)
                rts_error(VARLSTCNT(1) ERR_ZGOTOTOOBIG);
        for (unwind = 0, fp = frame_pointer; fp->old_frame_pointer; fp = fp->old_frame_pointer)
        {
                if ((fp->type & SFT_COUNT) && (level == zlevel--))
			break;
		unwind++;
        }
        for (ret_targ = NULL; unwind--; )
        {
                while (tp_pointer && tp_pointer->fp <= frame_pointer)
                        op_trollback(-1);
                if (0 == unwind)
                        ret_targ = get_ret_targ();
                op_unwind();
        }
        if (NULL != ret_targ)
        {
                *ret_targ = literal_null;
                ret_targ->mvtype |= MV_RETARG;
        }
        return;
}
