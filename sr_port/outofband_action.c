/****************************************************************
 *								*
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "outofband.h"
#include "libyottadb_int.h"

GBLREF io_pair		io_std_device;
GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*restart_ctxt, *restart_pc;
GBLREF void             (*tp_timeout_action_ptr)(void);
GBLREF volatile int4 	ctrap_action_is, outofband;

error_def(ERR_CTRAP);
error_def(ERR_CTRLC);
error_def(ERR_CTRLY);
error_def(ERR_JOBINTRRQST);

void outofband_action(boolean_t lnfetch_or_start)
{
	if (outofband)
	{
		if (io_std_device.in->type == tt)
			iott_flush(io_std_device.in);
		if (lnfetch_or_start)
		{
			restart_pc =  frame_pointer->mpc;
			restart_ctxt = frame_pointer->ctxt;
		}
		switch(outofband)
		{
			case (ctrly):		/* This signal is ignored in simpleAPI */
				if (!(IS_SIMPLEAPI_MODE))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLY);
				break;
			case (ctrlc):
				/* Note this outofband is currently allowed for simpleAPI functions.
				 * It exits the process in simpleAPI mode.
				 */
				if (!(IS_SIMPLEAPI_MODE))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
				else
					exit(ERR_CTRLC);
				break;
			case (ctrap):		/* This signal is ignored in simpleAPI */
				if (!(IS_SIMPLEAPI_MODE))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_CTRAP, 1, ctrap_action_is);
				break;
			case (tptimeout):
				/* Currently following is nothing but an rts_error.
				 * Function pointer is used for flexibility.
				 * Note this outofband is currently allowed for simpleAPI functions.
				 */
				(*tp_timeout_action_ptr)();
				break;
			case (jobinterrupt):	/* This signal is ignored in simpleAPI */
				if (!(IS_SIMPLEAPI_MODE))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBINTRRQST);
				break;
			default:
				assertpro(FALSE);
				break;
		}
	}
}
