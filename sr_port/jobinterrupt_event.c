/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* job interrupt event - an interrupt has been requested.

   - Call xfer_set_handlers so next M instruction traps to interrupt routine
   - Other required housecleaning for VMS.

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

GBLREF	xfer_entry_t		xfer_table[];
GBLREF	volatile int4 		outofband;
GBLREF	volatile boolean_t	dollar_zininterrupt;

/* Routine called when an interrupt event occurs (signaled by mupip intrpt or other future method
 * of signaling interrupts). This code is driven as a signal handler on Unix and from the START_CH
 * macro on VMS where it intercepts the posix signal.
 */
UNIX_ONLY(void jobinterrupt_event(int sig, siginfo_t *info, void *context))
VMS_ONLY(void jobinterrupt_event(void))
{
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig);
	/* Note the (presently unused) args are to match signature for signal handlers in Unix */
	if (!dollar_zininterrupt)
		(void)xfer_set_handlers(outofband_event, &jobinterrupt_set, 0);
}

/* Call back routine from xfer_set_handlers to complete outofband setup */
void jobinterrupt_set(int4 dummy_val)
{
#	ifdef VMS
	int4 status;

	status = sys$setef(efn_outofband);
	assert(SS$_WASCLR == status);
	if ((SS$_WASCLR != status) && (SS$_WASSET != status))
		GTMASSERT;
#	endif
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
	VMS_ONLY(sys$wake(0,0));
}
