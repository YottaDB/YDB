/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "io.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "have_crit.h"
#include "deferred_events_queue.h"

GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF io_pair			io_std_device;
GBLREF stack_frame		*frame_pointer;
GBLREF unsigned char		*restart_ctxt, *restart_pc;
GBLREF void			(*tp_timeout_action_ptr)(void);
GBLREF volatile int4		outofband;
GBLREF void			(*ztimeout_action_ptr)(void);

error_def(ERR_CTRAP);
error_def(ERR_CTRLC);
error_def(ERR_TERMHANGUP);
error_def(ERR_TERMWRITE);
error_def(ERR_JOBINTRRQST);

void outofband_action(boolean_t lnfetch_or_start)
{	/* initates transfer of control using the condition handler mechanism expecting a catch by the mdeb_condition_handler */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (outofband)
	{
		if (io_std_device.in->type == tt)
			iott_flush(io_std_device.in);
		if (lnfetch_or_start)
		{
			frame_pointer->restart_pc = frame_pointer->mpc;
			frame_pointer->restart_ctxt = frame_pointer->ctxt;
		}
		switch(outofband)
		{
			case ctrlc:
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_CTRLC);
				break;
			case ctrap:
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_CTRAP, 1, TAREF1(save_xfer_root, ctrap).param_val);
				break;
			case sighup:
				TAREF1(save_xfer_root, sighup).event_state = pending;
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TERMHANGUP);
				break;
			case tptimeout:	/* following is basically an rts_error; function pointer is used for flexibility */
				(*tp_timeout_action_ptr)();
				break;
			case jobinterrupt:
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_JOBINTRRQST);
				break;
			case ztimeout: /* following is basically rts_error */
				(*ztimeout_action_ptr)();
				break;
			case ttwriterr:
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_TERMWRITE, 0, TAREF1(save_xfer_root, ttwriterr).param_val);
				break;
			default:
				assertpro(FALSE && outofband);
				break;
		}
	}
}
