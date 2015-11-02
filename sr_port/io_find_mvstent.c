/****************************************************************
 *                                                              *
 *      Copyright 2007, 2009 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"

GBLREF  mv_stent                *mv_chain;
GBLREF  stack_frame             *frame_pointer;
GBLREF  unsigned char           *stackbase, *stacktop, *msp, *stackwarn;

/* Find and optionally clear the mv_stent keeping interrupted device information for us */
mv_stent *io_find_mvstent(io_desc *io_ptr, boolean_t clear_mvstent)
{
        mv_stent        *mvc, *mv_zintdev;

        assert(msp <= stackbase && msp > stacktop);
        assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
        assert(frame_pointer <= (stack_frame *)stackbase && frame_pointer > (stack_frame *)stacktop);
        mv_zintdev = NULL;
        for (mvc = mv_chain; mvc < (mv_stent *)frame_pointer ; mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc))
        {
                if (MVST_ZINTDEV == mvc->mv_st_type && io_ptr == mvc->mv_st_cont.mvs_zintdev.io_ptr)
                {
                        mv_zintdev = mvc;
                        break;
                }
                if (!mvc->mv_st_next)
                        GTMASSERT;
        }
        if (mv_zintdev && clear_mvstent)
        {
                if (mv_chain == mv_zintdev)
                        POP_MV_STENT();         /* just pop if top of stack */
		else
		{
			mv_zintdev->mv_st_cont.mvs_zintdev.io_ptr = NULL;
			mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
		}
        }
        return mv_zintdev;
}
