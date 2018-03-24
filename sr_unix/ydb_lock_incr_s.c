/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "libyottadb_int.h"
#include "error.h"
#include "op.h"
#include "stringpool.h"
#include "callg.h"
#include "mvalconv.h"
#include "namelook.h"
#include "outofband.h"

GBLREF	volatile int4	outofband;

/* Routine to incrementally obtain a lock (not unlocking everything first).
 *
 * Parameters:
 *   - timeout_nsec - Causes the lock attempt to timeout after the given time (in nanoseconds) has elapsed.
 *   - varname      - Contains the basevar name (local or global). Must be a valid variable name.
 *   - subs_used    - The number of subscripts specified in the subsarray parm
 *   - subsarray    - An array of 'subs_used' ydb_buffer_t structures containing the definitions of the subscripts.
 */
int ydb_lock_incr_s(unsigned long long timeout_nsec, ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray)
{
	va_list			var;
	int			parmidx, timeoutms, lock_rc;
	gparam_list		plist;
	boolean_t		error_encountered;
	mval			timeout_mval, varname_mval;
	mval			plist_mvals[YDB_MAX_SUBS + 1];
	unsigned long long	timeout_sec;
	ydb_var_types		var_type;
	int			var_svn_index;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_LOCK_INCR);		/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		LIBYOTTADB_DONE;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	ISSUE_TIME2LONG_ERROR_IF_NEEDED(timeout_nsec);
	/* Setup and validate the varname */
	VALIDATE_VARNAME(varname, var_type, var_svn_index, FALSE);
	/* ISV references are not supported for this call */
	if (LYDB_VARREF_ISV == var_type)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
	/* Initialize the private lock list */
	op_lkinit();
	/* Setup parameter list for callg() invocation of op_lkname() */
	plist.arg[0] = NULL;				/* First arg is extended reference that simpleAPI doesn't support */
	varname_mval.mvtype = MV_STR;
	varname_mval.str.addr = varname->buf_addr;	/* Second arg is varname */
	varname_mval.str.len = varname->len_used;
	plist.arg[1] = &varname_mval;
	/* Note, no rebuffering is needed here as the lock name and subscripts are all copied into the private
	 * lock block space by routines called by op_lkname so are effectively already rebuffered. No need for
	 * us to do it again.
	 */
	COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 2, "ydb_lock_incr_s()");
	callg((callgfnptr)op_lkname, &plist);
	/* At this point, the private lock block has been created. Remaining task before calling "op_incrlock" is to
	 * convert the timeout value from nanoseconds to seconds
	 */
	assert(MAXPOSINT4 >= (timeout_nsec / NANOSECS_IN_MSEC));	/* Or else a TIME2LONG error would have been issued above */
	timeout_sec = (timeout_nsec / NANOSECS_IN_SEC);
	i2mval(&timeout_mval, (int)timeout_sec);
	lock_rc = op_incrlock(&timeout_mval);
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* The counter should have never become non-zero in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return lock_rc ? YDB_OK : YDB_LOCK_TIMEOUT;
}
