/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmmsg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "anticipatory_freeze.h"

GBLREF	int		gtm_errno;
GBLREF	boolean_t 	created_core;
GBLREF	boolean_t	dont_want_core;
GBLREF	gd_region	*gv_cur_region;
GBLREF	jnlpool_addrs	jnlpool;

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

int rts_error_va(void *csa, int argcnt, va_list var);

/* ----------------------------------------------------------------------------------------
 *  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 * ----------------------------------------------------------------------------------------
 */

int rts_error(int argcnt, ...)
{
	va_list		var;
	sgmnt_addrs	*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = (ANTICIPATORY_FREEZE_AVAILABLE && jnlpool.jnlpool_ctl) ? REG2CSA(gv_cur_region) : NULL;
	VAR_START(var, argcnt);
	return rts_error_va(csa, argcnt, var);
}

int rts_error_csa(void *csa, int argcnt, ...)
{
	va_list		var;

	VAR_START(var, argcnt);
	return rts_error_va(csa, argcnt, var);
}

int rts_error_va(void *csa, int argcnt, va_list var)
{
	int 		msgid;
	va_list		var_dup;
	const err_msg	*msg;
	const err_ctl	*ctl;
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
	    || (ERR_JOBINTRRETHROW == msgid))
		error_condition = msgid;
	else
	{	/* Note this message is not flushed out. This is so user console is not polluted with messages that are going to be
		 * handled by a ZTRAP. If ZTRAP is not active, the message will be flushed out in mdb_condition_handler - which is
		 * usually the top level handler or is rolled over into by higher handlers.
		 */
		if (IS_GTMSECSHR_IMAGE)
			util_out_print(NULL, RESET);
		if (NULL == (ctl = err_check(msgid)))
			msg = NULL;
		else
			GET_MSG_INFO(msgid, ctl, msg);
		error_condition = msgid;
		severity = NULL == msg ? ERROR : SEVMASK(msgid);
		gtm_putmsg_list(csa, argcnt, var);
		if (DUMPABLE)
			created_core = dont_want_core = FALSE;		/* We can create a(nother) core now */
		if (IS_GTMSECSHR_IMAGE)
			util_out_print(NULL, OPER);			/* gtmsecshr errors always immediately pushed out */
	}
	va_end(var_dup);
	va_end(var);
	DRIVECH(msgid);				/* Drive the topmost (inactive) condition handler */
	/* Note -- at one time there was code here to catch if we returned from the condition handlers
	 * when the severity was error or above. That code had to be removed because of several errors
	 * that are handled and returned from. An example is EOF errors.  SE 9/2000
	 */
	return 0;
}
