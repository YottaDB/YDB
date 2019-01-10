/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* job interrupt event - an interrupt has been requested.

   - Call xfer_set_handlers so next M instruction traps to interrupt routine

*/

#include "mdef.h"

#ifdef UNIX
#  include <signal.h>
#else
#  include <ssdef.h>
#  include "efn.h"
#endif
#ifdef GTM_PTHREAD
#  include <pthread.h>
#endif

#include "gtm_stdio.h"
#include "io.h"
#include "op.h"
#include "xfer_enum.h"
#include "outofband.h"
#include "deferred_events.h"
#include "jobinterrupt_event.h"
#include "fix_xfer_entry.h"
#include "generic_signal_handler.h"

GBLREF	xfer_entry_t		xfer_table[];
GBLREF	volatile int4 		outofband;
GBLREF	volatile boolean_t	dollar_zininterrupt;

/* Routine called when an interrupt event occurs (signaled by mupip intrpt or other future method
 * of signaling interrupts). This code is driven as a signal handler on Unix where it intercepts the posix signal.
 */
void jobinterrupt_event(int sig, siginfo_t *info, void *context)
{
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig, IS_EXI_SIGNAL_FALSE, NULL, NULL);
	/* Note the (presently unused) args are to match signature for signal handlers in Unix */
	if (!dollar_zininterrupt)
		(void)xfer_set_handlers(outofband_event, &jobinterrupt_set, 0);
}

/* Call back routine from xfer_set_handlers to complete outofband setup */
void jobinterrupt_set(int4 dummy_val)
{
	if (jobinterrupt != outofband)
	{	/* We need jobinterrupt out of band processing at our earliest convenience */
		outofband = jobinterrupt;
                FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);
                FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);
                FIX_XFER_ENTRY(xf_zbfetch, op_fetchintrrpt);
                FIX_XFER_ENTRY(xf_zbstart, op_startintrrpt);
                FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);
                FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);
	}
}
