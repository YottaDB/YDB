/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "gtm_signal.h"

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
	struct sigaction new_action;

	SETUP_THREADGBL_ACCESS;
	/* Setup new signal handler to just drive condition handler which will do the right thing.  */
	memset(&new_action, 0, SIZEOF(new_action));
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;
	new_action.sa_sigaction = jobinterrupt_event;
	sigaction(SIGUSR1, &new_action, NULL);

	/* Provide initial setting for $ZINTERRUPT */
	envvar_logical.addr = GTM_ZINTERRUPT;
	envvar_logical.len = SIZEOF(GTM_ZINTERRUPT) - 1;
	if ((SS_NORMAL != TRANS_LOG_NAME(&envvar_logical, &dollar_zinterrupt.str, trans_bufr, SIZEOF(trans_bufr),
						do_sendmsg_on_log2long)) || (0 == dollar_zinterrupt.str.len))
	{	/* Translation failed - use default */
		dollar_zinterrupt.str.addr = DEF_ZINTERRUPT;
		dollar_zinterrupt.str.len = SIZEOF(DEF_ZINTERRUPT) - 1;
	} else	/* put value in stringpool if translation succeeded */
		s2pool(&dollar_zinterrupt.str);
	dollar_zinterrupt.mvtype = MV_STR;
	return;
}
