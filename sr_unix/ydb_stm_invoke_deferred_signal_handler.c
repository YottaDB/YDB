/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "generic_signal_handler.h"
#include "sig_init.h"
#include "continue_handler.h"
#include "ctrlc_handler.h"
#include "jobinterrupt_event.h"
#include "suspsigs_handler.h"
#include "jobexam_signal_handler.h"
#include "jobsp.h"
#include "op_fnzpeek.h"

void ydb_stm_invoke_deferred_signal_handler()
{	/* Below code assumes there are only 6 signal handler types in "enum sig_handler_t". Assert that.
	 * Fix below assert and following code if/when assert fails to take any new handler types into account.
	 */
	assert(0 == sig_hndlr_none);
	assert(11 == sig_hndlr_num_entries);
	/* Note: The STAPI_CLEAR_SIGNAL_HANDLER_DEFERRED call for each of the deferred signal handler
	 * invocation done below is taken care of by the FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED invocation
	 * done inside each of these signal handlers at function entry.
	 */
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_continue_handler))
	{
		assert(SIGCONT == stapi_signal_handler_oscontext[sig_hndlr_continue_handler].sig_num);
		continue_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_ctrlc_handler))
	{
		assert(SIGINT == stapi_signal_handler_oscontext[sig_hndlr_ctrlc_handler].sig_num);
		ctrlc_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	/* Since dbcertify operates as a standalone tool, it does not have multiple threads and so signal
	 * handling should never be deferred/forwarded there. Assert that below. So no need to handle it here.
	 */
	assert(!STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_dbcertify_signal_handler));
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_generic_signal_handler))
	{
		generic_signal_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_jobexam_signal_handler))
	{
		assert((SIGBUS== stapi_signal_handler_oscontext[sig_hndlr_jobexam_signal_handler].sig_num)
			|| (SIGSEGV
				== stapi_signal_handler_oscontext[sig_hndlr_jobexam_signal_handler].sig_num));
		jobexam_signal_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_jobinterrupt_event))
	{
		assert(SIGUSR1 == stapi_signal_handler_oscontext[sig_hndlr_jobinterrupt_event].sig_num);
		jobinterrupt_event(DUMMY_SIG_NUM, NULL, NULL);
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_job_term_handler))
	{
		assert(SIGTERM == stapi_signal_handler_oscontext[sig_hndlr_job_term_handler].sig_num);
		job_term_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_op_fnzpeek_signal_handler))
	{
		assert(SIGTERM == stapi_signal_handler_oscontext[sig_hndlr_op_fnzpeek_signal_handler].sig_num);
		op_fnzpeek_signal_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_suspsigs_handler))
	{
		assert((SIGTTIN == stapi_signal_handler_oscontext[sig_hndlr_suspsigs_handler].sig_num)
			|| (SIGTTOU == stapi_signal_handler_oscontext[sig_hndlr_suspsigs_handler].sig_num)
			|| (SIGTSTP == stapi_signal_handler_oscontext[sig_hndlr_suspsigs_handler].sig_num));
		suspsigs_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_timer_handler))
	{
		assert(SIGALRM == stapi_signal_handler_oscontext[sig_hndlr_timer_handler].sig_num);
		timer_handler(DUMMY_SIG_NUM, NULL, NULL);
	}
	CLEAR_DEFERRED_STAPI_CHECK_NEEDED;
}
