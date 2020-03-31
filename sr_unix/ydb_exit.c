/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/mman.h>

#include "libyottadb_int.h"
#include "invocation_mode.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "tp_frame.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#include "gtm_trigger.h"
#endif
#include "op.h"
#include "gtmci.h"
#include "gtm_exit_handler.h"
#include "dlopen_handle_array.h"

GBLREF	int			mumps_status;
GBLREF	struct sigaction	orig_sig_action[];
GBLREF	boolean_t		simpleThreadAPI_active;
GBLREF	uint4			dollar_tlevel;
GBLREF	int4			exi_condition;
#ifdef DEBUG
GBLREF	pthread_t		ydb_stm_worker_thread_id;
#endif
OS_PAGE_SIZE_DECLARE

/* Routine exposed to call-in user to exit from active YottaDB environment */
int ydb_exit()
{
	int			status, sig, save_errno, rc;
	pthread_t		thisThread, threadid;
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	boolean_t		error_encountered;
        DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;
	if (!ydb_init_complete)
		return YDB_OK;		/* If we aren't initialized, we don't have things to take down so just return */
	if (dollar_tlevel && simpleThreadAPI_active)
	{	/* We are inside TP. If $TLEVEL is 2 (i.e. TP depth is 2), in SimpleThreadAPI mode, we cannot differentiate
		 * if this "ydb_exit" call is happening inside the 1st level TP callback function or a 2nd level TP callback
		 * function (since "ydb_exit" does not have a "tptoken" parameter). In this case, the thread executing the
		 * 2nd level TP callback function is the one holding the current YottaDB engine multi-thread lock and so
		 * the thread invoking "ydb_exit" from the 1st level TP callback function should not be allowed to fall
		 * through to the below code where the threaded_api_ydb_engine_lock macro call would deadlock. Hence return
		 * an error in this case. Reuse a pre-existing error that does not allow "ydb_exit" to be called in the
		 * middle of call-ins. Since we do not necessarily hold the YottaDB engine multi-thread lock at this point,
		 * we cannot do any ESTABLISH_RET/rts_error_csa etc. calls. Hence just the return of an error code
		 * (i.e. no populating $zstatus etc. so "ydb_zstatus" will not correspond to this error code etc.).
		 * "ydb_exit" returns positive error code so return ERR_INVYDBEXIT, not YDB_ERR_INVYDBEXIT below.
		 */
		return ERR_INVYDBEXIT;
	}
	threaded_api_ydb_engine_lock(YDB_NOTTP, NULL, LYDB_RTN_NONE, &save_active_stapi_rtn, &save_errstr, &get_lock, &status);
	if (0 != status)
	{
		assert(0 < status);	/* i.e. can only be a system error, not a YDB_ERR_* error code */
		return status;		/* Lock failed - no condition handler yet so just return the error code */
	}
	if (!ydb_init_complete)
	{	/* "ydb_init_complete" was TRUE before we got the "threaded_api_ydb_engine_lock" lock but became FALSE
		 * afterwards. This implies some other concurrent thread did the "ydb_exit" so we can return from this
		 * "ydb_exit" call without doing anything more.
		 */
		DBGSIGHND((stderr, "ydb_exit(): ydb_exit() entered -- nothing to do\n"));
	} else
	{
		DBGSIGHND((stderr, "ydb_exit(): ydb_exit() entered -- engine shutdown commencing\n"));
		/* If this is a SimpleThreadAPI environment and we hold the YottaDB engine multi-thread mutex lock (obtained
		 * above in the threaded_api_ydb_engine_lock call). So we can proceed with exit handling. We are also
		 * guaranteed this thread is not the MAIN worker thread (asserted below).
		 */
		assert(!simpleThreadAPI_active
		       || (ydb_stm_worker_thread_id && !pthread_equal(pthread_self(), ydb_stm_worker_thread_id)));
		ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
		if (error_encountered)
		{	/* "ydb_simpleapi_ch" encountered an error and transferred control back here.
			 * Return after mutex lock cleanup.
			 */
			threaded_api_ydb_engine_unlock(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
			REVERT;
			/* "ydb_exit" returns positive error code so return TREF(ydb_error_code) as is
			 * (i.e. no negation for YDB_ERR_* like is common in other ydb_*_s() function calls)
			 */
			assert(0 < TREF(ydb_error_code));
			return TREF(ydb_error_code);
		}
		if (dollar_tlevel)
		{	/* Cannot take down YottaDB environment while inside an active TP transaction. Issue error. */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVYDBEXIT);
		}
		assert(NULL != frame_pointer);
		/* If process_exiting is set (and the YottaDB environment is still active since "ydb_init_complete" is TRUE
		 * here), shortcut some of the checks and cleanups we are making in this routine as they are not
		 * particularly useful. If the environment is not active though, that's still an error.
		 */
		if (!process_exiting)
		{	/* Do not allow ydb_exit() to be invoked from external calls (unless process_exiting) */
			if (!(SFT_CI & frame_pointer->type) || !(MUMPS_CALLIN & invocation_mode)
			    || (1 < TREF(gtmci_nested_level)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVYDBEXIT);
			/* Now get rid of the whole M stack - end of YottaDB environment */
			while (NULL != frame_pointer)
			{
				while ((NULL != frame_pointer) && !(frame_pointer->type & SFT_CI))
				{
					if (SFT_TRIGR & frame_pointer->type)
						gtm_trigger_fini(TRUE, FALSE);
					else
						op_unwind();
				}
				if (NULL != frame_pointer)
				{	/* unwind the current invocation of call-in environment */
					assert(frame_pointer->type & SFT_CI);
					ci_ret_code_quit();
				}
			}
		}
		gtm_exit_handler(); /* rundown all open database resource */
		/* Note - we used to restore the alternate stack (altstackptr) here to what was used before but this
		 * caused issues when multiple threads were shutting down suddenly (like with a signal). In this
		 * situation, this routine can run while other threads are still active. It is not predictably the
		 * last thing to run. In this situation, restoring the old altsig handler did so while the previous
		 * altstack was still in use in some thread causing a sig-11 when that thread came back and tried to
		 * reference its now non-existent stack.
		 */
		REVERT;
		/* Restore the signal handlers that were saved and overridden during ydb_init()->gtm_startup()->sig_init() */
		if (!USING_ALTERNATE_SIGHANDLING)
		{	/* Only done if using "regular" signal handling (done by YDB instead of main language) */
			for (sig = 1; sig <= NSIG; sig++)
				sigaction(sig, &orig_sig_action[sig], NULL);
		} else
			sigaction(YDBSIGNOTIFY, &orig_sig_action[YDBSIGNOTIFY], NULL);	/* Restore one signal handler we used */
		/* We might have opened one or more shlib handles using "dlopen". Do a "dlclose" of them now. */
		dlopen_handle_array_close();
		ydb_init_complete = FALSE;
	}
	threaded_api_ydb_engine_unlock(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
	return YDB_OK;
}
