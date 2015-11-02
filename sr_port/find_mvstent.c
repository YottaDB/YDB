/****************************************************************
 *                                                              *
 *      Copyright 2011 Fidelity Information Services, Inc *
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
#include <rtnhdr.h>
#include "mv_stent.h"
#include "find_mvstent.h"
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

/* Find and optionally clear the mv_stent keeping information for interrupted
 * timed non IO commands.  Unlike IO commands there is no associated structure
 * so the restart_pc is used to identify which instance of the command is of
 * interest */
mv_stent *find_mvstent_cmd(zintcmd_ops match_command, unsigned char *match_restart_pc, unsigned char *match_restart_ctxt,
	boolean_t clear_mvstent)
{
        mv_stent        *mvc, *mv_zintcmd;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
        assert(msp <= stackbase && msp > stacktop);
        assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
        assert(frame_pointer <= (stack_frame *)stackbase && frame_pointer > (stack_frame *)stacktop);
	assert((0 < match_command) && (ZINTCMD_LAST > match_command));
        mv_zintcmd = NULL;
	if ((0 >= TAREF1(zintcmd_active, match_command).count)	/* at least one mv_stent for this command */
		|| (match_restart_pc != TAREF1(zintcmd_active, match_command).restart_pc_last)
		|| (match_restart_ctxt != TAREF1(zintcmd_active, match_command).restart_ctxt_last))
		return mv_zintcmd;	/* not ours so no need to search stack */
        for (mvc = mv_chain; mvc < (mv_stent *)frame_pointer ; mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc))
        {
                if (MVST_ZINTCMD == mvc->mv_st_type && match_command == mvc->mv_st_cont.mvs_zintcmd.command
			&& match_restart_pc == mvc->mv_st_cont.mvs_zintcmd.restart_pc_check
			&& match_restart_ctxt == mvc->mv_st_cont.mvs_zintcmd.restart_ctxt_check)
                {
                        mv_zintcmd = mvc;
                        break;
                }
                if (!mvc->mv_st_next)
                        GTMASSERT;
        }
        if (mv_zintcmd && clear_mvstent)
        {	/* restore previous zintcmd_active values before clearing MV_STENT entry */
		TAREF1(zintcmd_active, match_command).restart_pc_last = mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_pc_prior;
		TAREF1(zintcmd_active, match_command).restart_ctxt_last = mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_ctxt_prior;
		TAREF1(zintcmd_active, match_command).count--;
		assert(0 <= TAREF1(zintcmd_active, match_command).count);
                if (mv_chain == mvc)
                        POP_MV_STENT();         /* just pop if top of stack */
		else
		{
			mv_zintcmd->mv_st_cont.mvs_zintcmd.command = ZINTCMD_NOOP;
			mv_zintcmd->mv_st_cont.mvs_zintcmd.restart_pc_check = NULL;
		}
        }
        return mv_zintcmd;
}
