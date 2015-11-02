/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_stdio.h"

#include "gtm_putmsg_list.h"
#include <errno.h>

#include "gtmimagename.h"
#include "error.h"
#include "util.h"

GBLREF	int		gtm_errno;
GBLREF	boolean_t 	created_core;
GBLREF	boolean_t	dont_want_core;

error_def(ERR_ASSERT);
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

/* ----------------------------------------------------------------------------------------
 *  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 * ----------------------------------------------------------------------------------------
 */
int rts_error(int argcnt, ...)
{
	int 		msgid;
	va_list		var;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
	if (-1 == gtm_errno)
		gtm_errno = errno;
	VAR_START(var, argcnt);
	msgid = va_arg(var, int);
	va_end(var);
	/* If there was a previous fatal error that did not yet get printed, do it before overwriting the
	 * util_output buffer with the about-to-be-handled nested error. This way one will see ALL the
	 * fatal error messages (e.g. assert failures) in the order in which they occurred instead of
	 * just the last nested one.
	 */
	if (DUMPABLE)
		PRN_ERROR;
	/* This is simply a place holder msg to signal tp restart or otherwise rethrow an error */
	if ((ERR_TPRETRY == msgid) || (ERR_REPEATERROR == msgid) || (ERR_REPLONLNRLBK == msgid) || (ERR_JOBINTRRQST == msgid)
	    || (ERR_JOBINTRRETHROW == msgid))
		error_condition = msgid;
	else
	{	/* Note this message is not flushed out. This is so user console is not polluted with messages that are going to be
		 * handled by a ZTRAP. If ZTRAP is not active, the message will be flushed out in mdb_condition_handler - which is
		 * usually the top level handler or is rolled over into by higher handlers.
		 */
		if (IS_GTMSECSHR_IMAGE)
			util_out_print(NULL, RESET);
		VAR_START(var, argcnt);		/* restart arg list */
		gtm_putmsg_list(argcnt, var);
		va_end(var);
		if (DUMPABLE)
			created_core = dont_want_core = FALSE;		/* We can create a(nother) core now */
		if (IS_GTMSECSHR_IMAGE)
			util_out_print(NULL, OPER);			/* gtmsecshr errors always immediately pushed out */
	}
	DRIVECH(msgid);				/* Drive the topmost (inactive) condition handler */
	/* Note -- at one time there was code here to catch if we returned from the condition handlers
	 * when the severity was error or above. That code had to be removed because of several errors
	 * that are handled and returned from. An example is EOF errors.  SE 9/2000
	 */
	return 0;
}
