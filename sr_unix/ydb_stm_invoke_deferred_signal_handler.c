/****************************************************************
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
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
#include "ydb_os_signal_handler.h"
#include "sig_init.h"
#include "continue_handler.h"
#include "ctrlc_handler.h"
#include "jobinterrupt_event.h"
#include "suspsigs_handler.h"
#include "jobexam_signal_handler.h"
#include "jobsp.h"
#include "op_fnzpeek.h"
#include "error.h"

GBLREF	enum sig_handler_t	ydb_stm_invoke_deferred_signal_handler_type;

/* Condition handler needed to reset the global variable that indicates we are inside "ydb_stm_invoke_deferred_signal_handler"
 * in case of an error that takes control out of this function before the function can clean up the global variable.
 * Note that for the case where no condition handler invocation happens (i.e. normal return case from
 * "ydb_stm_invoke_deferred_signal_handler") in the STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED macro.
 */
CONDITION_HANDLER(ydb_stm_invoke_deferred_signal_handler_ch)
{
	START_CH(TRUE);
	assert(ydb_stm_invoke_deferred_signal_handler_type);
	ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	NEXTCH;
}

void ydb_stm_invoke_deferred_signal_handler()
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (sig_hndlr_none != ydb_stm_invoke_deferred_signal_handler_type)
	{	/* We are already in a "ydb_stm_invoke_deferred_signal_handler" invocation.
		 * Return from a nested invocation of the same function.
		 */
		return;
	}
	ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_num_entries;
		/* above is needed to ensure the ENABLE_INTERRUPTS macro in ESTABLISH does not invoke "deferred_signal_handler"
		 * as that would in turn invoke STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED which would come back here
		 * leading to an indefinite recursion with no actual signal handler invoked. Setting this to a value other
		 * than "sig_hndlr_none" would cause the nested invocation to return at the top of this function thereby avoiding
		 * any recursion.
		 */
	ESTABLISH(ydb_stm_invoke_deferred_signal_handler_ch);
	ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	/* Below code assumes there are only 11 signal handler types in "enum sig_handler_t". Assert that.
	 * Fix below assert and following code if/when assert fails to take any new handler types into account.
	 */
	assert(0 == sig_hndlr_none);
	assert(11 == sig_hndlr_num_entries);
	/* Note: The STAPI_CLEAR_SIGNAL_HANDLER_DEFERRED call for each of the deferred signal handler
	 * invocation done below is taken care of by the FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED invocation
	 * done inside each of these signal handlers at function entry.
	 */
	/* Invoke "generic_signal_handler" first as that handles a process-terminating signal.
	 * If this signal is pending, invoking the handler will most likely terminate the process.
	 * No need to check for other deferred signal types.
	 *
	 * Note, several of the asserts below had '|| USING_ALTERNATE_SIGHANDLING' added to them. This is because the
	 * stapi_signal_handler_oscontext[] array is maintained by FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED() macro which
	 * is not used in alternate signal handling.
	 */
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_generic_signal_handler))
	{
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_generic_signal_handler;
		generic_signal_handler(DUMMY_SIG_NUM, NULL, NULL, IS_OS_SIGNAL_HANDLER_FALSE);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_continue_handler))
	{
		assert((SIGCONT == stapi_signal_handler_oscontext[sig_hndlr_continue_handler].sig_num)
		       || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_continue_handler;
		continue_handler(DUMMY_SIG_NUM, NULL, NULL);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_ctrlc_handler))
	{
		assert((SIGINT == stapi_signal_handler_oscontext[sig_hndlr_ctrlc_handler].sig_num) || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_ctrlc_handler;
		ctrlc_handler(DUMMY_SIG_NUM, NULL, NULL);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	/* Since dbcertify operates as a standalone tool, it does not have multiple threads and so signal
	 * handling should never be deferred/forwarded there. Assert that below. So no need to handle it here.
	 */
	assert(!STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_dbcertify_signal_handler));
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_jobexam_signal_handler))
	{
		assert((SIGBUS == stapi_signal_handler_oscontext[sig_hndlr_jobexam_signal_handler].sig_num)
		       || (SIGSEGV == stapi_signal_handler_oscontext[sig_hndlr_jobexam_signal_handler].sig_num)
		       || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_jobexam_signal_handler;
		jobexam_signal_handler(DUMMY_SIG_NUM, NULL, NULL);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_jobinterrupt_event))
	{
		assert((SIGUSR1 == stapi_signal_handler_oscontext[sig_hndlr_jobinterrupt_event].sig_num)
		       || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_jobinterrupt_event;
		jobinterrupt_event(DUMMY_SIG_NUM, NULL, NULL);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_job_term_handler))
	{
		assert((SIGTERM == stapi_signal_handler_oscontext[sig_hndlr_job_term_handler].sig_num)
		       || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_job_term_handler;
		job_term_handler(DUMMY_SIG_NUM, NULL, NULL);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_op_fnzpeek_signal_handler))
	{
		assert((SIGTERM == stapi_signal_handler_oscontext[sig_hndlr_op_fnzpeek_signal_handler].sig_num)
		       || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_op_fnzpeek_signal_handler;
		op_fnzpeek_signal_handler(DUMMY_SIG_NUM, NULL, NULL);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_suspsigs_handler))
	{
		assert((SIGTTIN == stapi_signal_handler_oscontext[sig_hndlr_suspsigs_handler].sig_num)
		       || (SIGTTOU == stapi_signal_handler_oscontext[sig_hndlr_suspsigs_handler].sig_num)
		       || (SIGTSTP == stapi_signal_handler_oscontext[sig_hndlr_suspsigs_handler].sig_num)
		       || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_suspsigs_handler;
		suspsigs_handler(DUMMY_SIG_NUM, NULL, NULL);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	if (STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_hndlr_timer_handler))
	{
		assert((SIGALRM == stapi_signal_handler_oscontext[sig_hndlr_timer_handler].sig_num)
		       || USING_ALTERNATE_SIGHANDLING);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_timer_handler;
		timer_handler(DUMMY_SIG_NUM, NULL, NULL, IS_OS_SIGNAL_HANDLER_FALSE);
		ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
	}
	CLEAR_DEFERRED_STAPI_CHECK_NEEDED;
	ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_num_entries;	/* See similar code before ESTABLISH above for WHY
										 * this is needed.
										 */
	REVERT;
	ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;
}
