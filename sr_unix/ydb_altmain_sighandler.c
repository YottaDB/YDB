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

#include "mdef.h"

#include "sig_init.h"
#include "generic_signal_handler.h"
#include "ydb_os_signal_handler.h"
#include "libyottadb.h"

GBLREF	volatile int4		exit_state;

/* Alternate signal handler for most fatal type signals - we are called from the dispatcher and call the regular handler but with
 * two NULL parms that are not used when doing alternate signal handling.
 */
int ydb_altmain_sighandler(int signum)
{
	generic_signal_handler(signum, NULL, NULL, IS_OS_SIGNAL_HANDLER_FALSE);
	if (IS_SIGNAL_DEFERRED)
		return YDB_DEFER_HANDLER;
	return YDB_OK;
}
