/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* While FIS did not write this module, it was derived from FIS code */

#include "mdef.h"

#include "gtm_signal.h"

#include "invocation_mode.h"
#include "gtmimagename.h"
#include "sig_init.h"
#include "generic_signal_handler.h"
#include "sighnd_debug.h"

GBLREF	struct sigaction	orig_sig_action[];

/* Provide null signal handler */
void null_handler(int sig, siginfo_t *info, void *context)
{	/* Just forward the signal if there's a handler for it and we are handling signals - otherwise ignore it */
	if (!USING_ALTERNATE_SIGHANDLING)
		DRIVE_NON_YDB_SIGNAL_HANDLER_IF_ANY("null_handler", sig, info, context, FALSE);
}
