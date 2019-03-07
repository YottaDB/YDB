/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <mdef.h>

#include <signal.h>
#ifdef GTM_PTHREAD
#  include <pthread.h>
#endif
#include "gtm_syslog.h"
#include "gtm_limits.h"

#include "gtmsiginfo.h"
#include "io.h"
#include "send_msg.h"
#include "setterm.h"
#include "continue_handler.h"
#include "gtmsecshr.h"
#include "generic_signal_handler.h"

GBLREF volatile int	is_suspended;
GBLREF io_pair		io_std_device;
GBLREF uint4		process_id;

error_def(ERR_REQ2RESUME);

void continue_handler(int sig, siginfo_t *info, void *context)
{
	gtmsiginfo_t	sig_info;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig, IS_EXI_SIGNAL_FALSE, NULL, NULL);
	/* Count how many times we get a continue-process signal (in DEBUG) */
	DEBUG_ONLY(DBGGSSHR((LOGFLAGS, "continue_handler: pid %d, continue_proc_cnt bumped from %d to %d\n",
			     process_id, TREF(continue_proc_cnt), TREF(continue_proc_cnt) + 1)));
	DEBUG_ONLY((TREF(continue_proc_cnt))++);
 	assert(SIGCONT == sig);
	/* Note - we do not forward this signal to the main routine but depend on our suspend/continue operations instead */
	extract_signal_info(sig, info, context, &sig_info);
	if (is_suspended)
	{	/* Don't bother checking if user info is available. If info isn't available,
		 * the value zero will be printed for pid and uid. Note that the following
		 * message was previously put out even when the process was not suspended but
		 * with the changes in spin locks that send continue messages "just in case",
		 * I thought it better to restrict this message to when the process was actually
		 * suspended and being continued. SE 7/01
		 */
		send_msg(VARLSTCNT(4) ERR_REQ2RESUME, 2, sig_info.send_pid, sig_info.send_uid);
	}
	is_suspended = FALSE;			/* Process is no longer suspended */
	CLEAR_DEFERRED_CTRLZ_CHECK_NEEDED;	/* Clear any pending deferred Ctrl-Z handling too */
}
