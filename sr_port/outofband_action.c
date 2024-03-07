/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	*
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
#include "stack_frame.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "libyottadb_int.h"
#include "have_crit.h"
#include "ztimeout_routines.h"

GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF io_pair			io_std_device;
GBLREF stack_frame		*frame_pointer;
GBLREF unsigned char		*restart_ctxt, *restart_pc;
GBLREF void			(*tp_timeout_action_ptr)(void);
GBLREF volatile int4		outofband;
GBLREF size_t			gtmMallocErrorSize;
GBLREF unsigned char		*gtmMallocErrorCallerid;

error_def(ERR_CTRAP);
error_def(ERR_CTRLC);
error_def(ERR_TERMHANGUP);
error_def(ERR_TERMWRITE);
error_def(ERR_JOBINTRRQST);

void outofband_action(boolean_t lnfetch_or_start)
{	/* initates transfer of control using the condition handler mechanism expecting a catch by the mdb_condition_handler */
	boolean_t	ours;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (outofband)
	{
		/* First check if a process terminating signal was deferred (that overloads "outofband" variable
		 * so various existing code paths automatically recognize the signal as an outofband event).
		 * If so handle just that and return and reset outofband to FALSE.
		 */
		if (deferred_signal == outofband)
		{
			DEFERRED_SIGNAL_HANDLING_CHECK;
			outofband_clear();
			assert(0 == outofband);
			return;
		}
		if (io_std_device.in->type == tt)
			iott_flush(io_std_device.in);
		if (lnfetch_or_start)
		{
			frame_pointer->restart_pc = frame_pointer->mpc;
			frame_pointer->restart_ctxt = frame_pointer->ctxt;
		}
		/* Check if any process terminating signal was received (e.g. SIGTERM, it would have set "outofband" to TRUE
		 * as part of the SET_FORCED_EXIT macro invocation). If so handle it now when it is safe to do so
		 * (i.e. while we are not inside a signal handler).
		 */
		switch(outofband)
		{
			case ctrlc:
				/* Note this outofband is currently allowed for simpleAPI functions.
				 * It exits the process in simple*API mode.
				 */
				if (!(IS_SIMPLEAPI_MODE))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_CTRLC);
				else
				{	/* If we are running with Go, just return as we'll panic instead of exit */
					if (YDB_MAIN_LANG_GO == ydb_main_lang)
						return;
					/* This exit() call is not ideal but there is no other well defined way to unwind
					 * intermixed YDB and non-YDB frames back to the shell or even some user stack level.
					 */
					exit(ERR_CTRLC);
				}
				break;
			case ctrap:		/* This signal is ignored in simpleAPI */
				if (!(IS_SIMPLEAPI_MODE))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_CTRAP, 1, TAREF1(save_xfer_root, ctrap).param_val);
				else
					outofband_clear();
				break;
			case sighup:
				TAREF1(save_xfer_root, sighup).event_state = pending;
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TERMHANGUP);
				break;
			case tptimeout:
				/* Following is basically an rts_error; function pointer is used for flexibility.
				 * Note this outofband is currently allowed for simpleAPI functions.
				 */
				(*tp_timeout_action_ptr)();
				break;
			case jobinterrupt:	/* This signal is ignored in simpleAPI */
				if (!(IS_SIMPLEAPI_MODE))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_JOBINTRRQST);
				else
					outofband_clear();
				break;
			case ztimeout: /* Following is basically rts_error (ignored for simpleAPI) */
				if (!(IS_SIMPLEAPI_MODE))
					ztimeout_action();
				else
					outofband_clear();
				break;
			case defer_error:
				ours = xfer_reset_if_setter(defer_error);
				assert(ours);
				if ((ERR_MALLOCCRIT == TAREF1(save_xfer_root, defer_error).param_val))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4)
						ERR_MALLOCCRIT, 2, gtmMallocErrorSize, gtmMallocErrorCallerid);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_TERMWRITE, 0,
								TAREF1(save_xfer_root, defer_error).param_val);
				break;
			default:
				assertpro(FALSE && outofband);
				break;
		}
	}
}
