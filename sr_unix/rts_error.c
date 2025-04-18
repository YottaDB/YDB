/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_syslog.h"
#include "gtm_pthread.h"
#include <stdarg.h>
#include <errno.h>

#include "gtm_multi_thread.h"
#include "gtm_putmsg_list.h"
#include "gtmimagename.h"
#include "error.h"
#include "util.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmmsg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "anticipatory_freeze.h"
#include "toktyp.h"
#include "cgp.h"
#include "stack_frame.h"
#include "get_syslog_flags.h"
#include "trace_table.h"
#include "caller_id.h"

GBLREF	boolean_t	first_syslog;					/* Global for a process - not thread specific */
GBLREF	boolean_t 	created_core;
GBLREF	boolean_t	dont_want_core;
GBLREF	boolean_t	run_time;
GBLREF	char		cg_phase;
GBLREF	stack_frame	*frame_pointer;
GBLREF	gd_region	*gv_cur_region;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	int		gtm_errno;
GBLREF	void		(*stx_error_va_fptr)(int in_error, ...);	/* Function pointer for stx_error_va() so this can avoid
								 	 * pulling stx_error() into gtmsecshr.								 	 					       */
GBLREF	uint4		process_id;

error_def(ERR_ASSERT);
error_def(ERR_DRVLONGJMP);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_JOBINTRRETHROW);
error_def(ERR_JOBINTRRQST);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_REPEATERROR);
error_def(ERR_REPLONLNRLBK);
error_def(ERR_STACKOFLOW);
error_def(ERR_TPRETRY);

int rts_error_va(void *csa, int argcnt, va_list var);

/* ----------------------------------------------------------------------------------------
 *  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 * ----------------------------------------------------------------------------------------
 */
/* coverity[+kill] */
int rts_error(int argcnt, ...)
{
	va_list		var;
	sgmnt_addrs	*csa;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* needed by PTHREAD_CSA_FROM_GV_CUR_REGION */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	PTHREAD_CSA_FROM_GV_CUR_REGION(csa, local_jnlpool);
	VAR_START(var, argcnt);
	return rts_error_va(csa, argcnt, var);
}

/* coverity[+kill] */
int rts_error_csa(void *csa, int argcnt, ...)
{
	va_list		var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, argcnt);
	return rts_error_va(csa, argcnt, var);
}

int rts_error_va(void *csa, int argcnt, va_list var)
{
	int 		msgid;
	va_list		var_dup;
	const err_ctl	*ctl;
	boolean_t	was_holder;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG
	if (TREF(rts_error_unusable) && !TREF(rts_error_unusable_seen))
	{
		TREF(rts_error_unusable_seen) = TRUE;
		/* The below assert ensures that this rts_error invocation is appropriate in the current context of the code that
		 * triggered this rts_error. If ever this assert fails, investigate the window of DBG_MARK_RTS_ERROR_UNUSABLE
		 * and DBG_MARK_RTS_ERROR_USABLE in the call-stack.
		 */
		assert(FALSE);
	}
#	endif
	if (MAX_RTS_ERROR_DEPTH < ++(TREF(rts_error_depth)))
	{	/* Too many errors nesting - stop it here - fatally. What we do is the following:
		 *   1. Put a message in syslog (not using send_msg_csa()) about what happened.
		 *   2. Put same message on stderr (straight shot - not through YottaDBs gtm_fprintf() routine)
		 *   3. Terminate this process with a core - not a fork as this process cannot continue.
		 *
		 * Note this counter is not an exact science. Normally when we drive condition handlers, we don't return here
		 * as some sort of error mitigation code runs and resets things. So the counter is typically ever increasing
		 * but there is a reset in mdb_condition_handler and ydb_simpleapi_ch - both handlers that "recover" and continue
		 * that clear this depth counter. So we are only detecting error loops in rts_error.c itself really (they typically
		 * occur in the PTHREAD_MUTEX_LOCK_IF_NEEDED() macro)) when/if they occur.
		 */
		if (first_syslog)
		{
			OPENLOG("YottaDB", get_syslog_flags(), LOG_USER);
			first_syslog = FALSE;
		}
		SYSLOG(LOG_USER | LOG_INFO, "%s", "%YDB-F-MAXRTSERRDEPTH Error loop detected - aborting image with core");
		fprintf(stderr, "%%YDB-F-MAXRTSERRDEPTH Error loop detected - aborting image with core");
		/* It is possible "ydb_dmp_tracetbl" gets a recursive error. Therefore have a safeguard to skip that step. */
		if (MAX_RTS_ERROR_DEPTH < (2 *(TREF(rts_error_depth))))
			ydb_dmp_tracetbl();
		DUMP_CORE;			/* Terminate *THIS* thread/process and produce a core */
	}
	PTHREAD_MUTEX_LOCK_IF_NEEDED(was_holder); /* get thread lock in case threads are in use */
	/* Note that rts_error does not return most of the times (control gets transferred somewhere else) so no point
	 * unlocking thread mutex at the end of this function. This means that if one thread gets an error, the rest of
	 * the threads might wait if/when they need to make a PTHREAD_MUTEX_LOCK_IF_NEEDED call. But that is considered
	 * okay since the process is anyways going to terminate due to the error in one thread. This is at least true
	 * for the current use of threads in mupip. If/when threads are used in GT.M this will need to be revisited.
	 */
	VAR_COPY(var_dup, var);
	if (-1 == gtm_errno)
		gtm_errno = errno;
	msgid = va_arg(var_dup, int);
	/* If there was a previous fatal error that did not yet get printed, do it before overwriting the
	 * util_output buffer with the about-to-be-handled nested error. This way one will see ALL the
	 * fatal error messages (e.g. assert failures) in the order in which they occurred instead of
	 * just the last nested one.
	 */
	if (DUMPABLE)
		PRN_ERROR;
	/* This is simply a place holder msg to signal tp restart or otherwise rethrow an error */
	if ((ERR_TPRETRY == msgid) || (ERR_REPEATERROR == msgid) || (ERR_REPLONLNRLBK == msgid) || (ERR_JOBINTRRQST == msgid)
			|| (ERR_JOBINTRRETHROW == msgid) || (ERR_DRVLONGJMP == msgid))
	{
		SET_ERROR_CONDITION(msgid);	/* sets "error_condition" & "severity" */
	} else
	{	/* Note this message is not flushed out. This is so user console is not polluted with messages that are going to be
		 * handled by a ZTRAP. If ZTRAP is not active, the message will be flushed out in mdb_condition_handler - which is
		 * usually the top level handler or is rolled over into by higher handlers.
		 */
		if (IS_GTMSECSHR_IMAGE)
			util_out_print(NULL, RESET);
		SET_ERROR_CONDITION(msgid);	/* sets "error_condition" & "severity" */
		if (!run_time && (CGP_PARSE == cg_phase) && !DUMP)
		{
			(*stx_error_va_fptr)(msgid, var_dup);
			if (0 < TREF(rts_error_depth))
				--(TREF(rts_error_depth));
			return FALSE;
		}
		/* The currently active condition handler is "ex_arithlit_compute_ch" which we know is okay with
		 * seeing runtime errors. In case of an error, it abandons the attempt to optimize literals and
		 * moves on. So we don't want to overwrite global variables TREF(util_outbuff) etc. with the to-be-abandoned
		 * error using the "gtm_putmsg_list()" call below. Hence the if check below.
		 */
		if ((NULL == active_ch) || !CHANDLER_EXISTS || (&ex_arithlit_compute_ch != active_ch->ch))
			gtm_putmsg_list(csa, argcnt, var);
		if (DUMPABLE)
			created_core = dont_want_core = FALSE;		/* We can create a(nother) core now */
		if (IS_GTMSECSHR_IMAGE)
			util_out_print(NULL, OPER);			/* gtmsecshr errors always immediately pushed out */
	}
	va_end(var_dup);
	va_end(var);
	DRIVECH(msgid);				/* Drive the topmost (inactive) condition handler */
	if (0 < TREF(rts_error_depth))
		--(TREF(rts_error_depth));
	/* Note -- at one time there was code here to catch if we returned from the condition handlers
	 * when the severity was error or above. That code had to be removed because of several errors
	 * that are handled and returned from. An example is EOF errors.  SE 9/2000
	 */
	return FALSE;
}

void assertfail(size_t testlen, char *teststr, int flen, char *fstr, int line) __attribute__ ((noreturn));
