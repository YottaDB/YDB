/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* gcc/Linux needs stdio.h before varargs until removed from error.h */
#include "gtm_stdio.h"
#include <varargs.h>
#include "gtm_putmsg_list.h"
#include <errno.h>

#include "error.h"
#include "util.h"

GBLREF int		gtm_errno;
GBLREF boolean_t 	created_core;
GBLREF boolean_t	dont_want_core;

#define FLUSH	1
#define RESET	2


/* ----------------------------------------------------------------------------------------
 *  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 * ----------------------------------------------------------------------------------------
 */

int rts_error(va_alist)
va_dcl
{
	int 		argcnt, msgid;
	va_list		var, varl;

	error_def(ERR_TPRETRY);
	error_def(ERR_ASSERT);
	error_def(ERR_GTMASSERT);
	error_def(ERR_ASSERT);
	error_def(ERR_GTMCHECK);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_OUTOFSPACE);

	if (-1 == gtm_errno)
		gtm_errno = errno;

	VAR_START(var);
	VAR_START(varl);
	argcnt = va_arg(varl, int);
	msgid = va_arg(varl, int);

	if (ERR_TPRETRY == msgid)		/* This is simply a place holder msg to signal tp restart */
	{
		error_condition = msgid;
		/* util_out_print(NULL, RESET);	Believe this is superfluous housecleaning. SE 9/2000 */
	} else
	{
		/* Note this message is not flushed out. This is so user console is not
		   polluted with messages that are going to be handled by a ZTRAP. If
		   ZTRAP is not active, the message will be flushed out in mdb_condition_handler
		   (which is usually the top level handler or is rolled over into by higher handlers. */
		gtm_putmsg_list(var);
		if (DUMPABLE)
			created_core = dont_want_core = FALSE;		/* We can create a(nother) core now */

	}

	DRIVECH(var);				/* Drive the topmost (inactive) condition handler */

	/* Note -- at one time there was code here to catch if we returned from the condition handlers
	   when the severity was error or above. That code had to be removed because of several errors
	   that are handled and returned from. An example is EOF errors.  SE 9/2000
	*/
	return 0;
}

