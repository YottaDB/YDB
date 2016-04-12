/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* job interrupt initialization. Accomplish following setups:

   - Setup handler for SIGUSR1 signal to be handled by jobinterrupt_event() (UNIX only).
   - Provide initial setting for $ZINTERRUPT from default or logical or
     environment variable if present.

*/

#include "mdef.h"

#include <signal.h>

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_logicals.h"
#include "trans_log_name.h"
#include "io.h"
#include "iosp.h"
#include "stringpool.h"
#include "jobinterrupt_init.h"
#include "jobinterrupt_event.h"

GBLREF	mval	dollar_zinterrupt;

#define DEF_ZINTERRUPT "IF $ZJOBEXAM()"

void jobinterrupt_init(void)
{
	mstr	envvar_logical;
	char	trans_bufr[MAX_TRANS_NAME_LEN];
	DCL_THREADGBL_ACCESS;
#ifdef UNIX
	struct sigaction new_action;

	SETUP_THREADGBL_ACCESS;
	/* Setup new signal handler to just drive condition handler which will do the right thing.
	   Note that although we use send a posix-style signal with mupip intrpt on VMS, the signal
	   that comes in is NOT handled by posix signal handler because the posix handler is implemented
	   using native condition handlers which interfere with GT.M's use of native condition handlers.
	   The VMS signal is instead intercepted via the START_CH macro on VMS which then drives the
	   jobinterrupt_event routine.
	*/
	memset(&new_action, 0, SIZEOF(new_action));
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;
#ifdef __sparc
	new_action.sa_handler = jobinterrupt_event;
#else
	new_action.sa_sigaction = jobinterrupt_event;
#endif
	sigaction(SIGUSR1, &new_action, NULL);
#else
	SETUP_THREADGBL_ACCESS;
	/* Handler for VMS setup via function pointer called by START_CH macro */
	RFPTR(gtm_sigusr1_handler) = jobinterrupt_event;
#endif

	/* Provide initial setting for $ZINTERRUPT */
	envvar_logical.addr = GTM_ZINTERRUPT;
	envvar_logical.len = SIZEOF(GTM_ZINTERRUPT) - 1;
	if (SS_NORMAL != TRANS_LOG_NAME(&envvar_logical, &dollar_zinterrupt.str, trans_bufr, SIZEOF(trans_bufr),
						do_sendmsg_on_log2long))
	{	/* Translation failed - use default */
		dollar_zinterrupt.str.addr = DEF_ZINTERRUPT;
		dollar_zinterrupt.str.len = SIZEOF(DEF_ZINTERRUPT) - 1;
	} else	/* put value in stringpool if translation succeeded */
		s2pool(&dollar_zinterrupt.str);
	dollar_zinterrupt.mvtype = MV_STR;
	return;
}
