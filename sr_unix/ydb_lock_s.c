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
#include "cmidef.h"
#include "cmmdef.h"
#include "stringpool.h"
#include "callg.h"
#include "mvalconv.h"
#include "namelook.h"
#include "outofband.h"

GBLREF	volatile int4	outofband;

/* Routine to obtain a lock (unlocking everything first).
 *
 * Parameters:
 *   - timeout_nsec - Causes the lock attempt to timeout after the given time (in nanoseconds) has elapsed.
 *   - namecount    - Count of "parm-triples" (varname, subcnt, sublist) or 0 if unlocking all.
 *   repeating parm-triples made up of:
 *   - varname	    - Lock varname
 *   - subs_used    - Number of subscripts
 *   - subsarray    - Array of subscripts
 */
int ydb_lock_s(unsigned long long timeout_nsec, int namecount, ...)
{
	va_list			var, varcpy;
	int			parmidx, lock_rc, sub_idx, var_svn_index;
	gparam_list		plist;
	boolean_t		error_encountered;
	mval			timeout_mval, varname_mval;
	mval			plist_mvals[YDB_MAX_SUBS + 1];
	ydb_buffer_t		*varname, *subsarray, *subptr;
	int			subs_used;
	unsigned long long	timeout_sec;
	ydb_var_types		var_type;
	char			buff[256];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_LOCK);			/* Note: macro could return from this function in case of errors */
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
	assert(MAXPOSINT4 == (YDB_MAX_TIME_NSEC / NANOSECS_IN_MSEC));
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	ISSUE_TIME2LONG_ERROR_IF_NEEDED(timeout_nsec);
	if (0 > namecount)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVNAMECOUNT, 2, RTS_ERROR_LITERAL("ydb_lock_s()"));
	if (YDB_MAX_NAMES < namecount)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_NAMECOUNT2HI, 3, RTS_ERROR_LITERAL("ydb_lock_s()"), YDB_MAX_NAMES);
	/* Need to validate all parms before we can do the unlock of all locks held by us */
	VAR_START(var, namecount);
	VAR_COPY(varcpy, var);		/* Used to validate parms, then var is used to process them */
	for (parmidx = 0; parmidx < namecount; parmidx++)
	{	/* Simplified version of the processing loop below that validates things */
		varname = va_arg(varcpy, ydb_buffer_t *);
		subs_used = va_arg(varcpy, int);
		if (0 > subs_used)
		{
			va_end(varcpy);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
		}
		subsarray = va_arg(varcpy, ydb_buffer_t *);
		if ((0 < subs_used) && (NULL == subsarray))
		{       /* Count of subscripts is non-zero but no subscript specified - error */
			va_end(varcpy);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SUBSARRAYNULL, 3, subs_used, LEN_AND_LIT("ydb_lock_s"));
		}
		/* Validate the varname */
		VALIDATE_VARNAME(varname, var_type, var_svn_index, FALSE);
		/* ISV references are not supported for this call */
		if (LYDB_VARREF_ISV == var_type)
		{
			va_end(varcpy);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
		}
		/* Now validate each subscript */
		for (sub_idx = 1, subptr = subsarray; sub_idx <= subs_used; sub_idx++, subptr++)
		{	/* Pull each subscript descriptor out of param list and put in our parameter buffer.
			 * A subscript has been specified - copy it to the associated mval and put its address
			 * in the param list. But before that, do validity checks on input ydb_buffer_t.
			 */
			if (IS_INVALID_YDB_BUFF_T(subptr))
			{
				SPRINTF(buff, "Invalid subsarray (index %d)", subptr - subsarray);
				va_end(varcpy);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					      LEN_AND_STR(buff), LEN_AND_LIT("op_lock_s()"));
			}
			CHECK_MAX_STR_LEN(subptr);
		}
	}
	va_end(varcpy);
	/* First step in this routine is to release all the locks */
	op_unlock();
	if (0 == namecount)
	{	/* If no names were specified, we're done after the unlock */
		LIBYOTTADB_DONE;
		REVERT;
		return YDB_OK;
	}
	/* Now we have one or more lock names to go through. First initialize the lock set for this new set of locks */
	op_lkinit();
	/* Now go through each nameset that has been passed to us sending each one to op_lkname() which creates a private
	 * lock block and accumulates these before passing the list to op_lock2 to actually perform the locking.
	 */
	varname_mval.mvtype = MV_STR;
	for (parmidx = 0; parmidx < namecount; parmidx++)
	{	/* Fetch our parms for this lock variable from the parm vector */
		varname = va_arg(var, ydb_buffer_t *);
		subs_used = va_arg(var, int);
		subsarray = va_arg(var, ydb_buffer_t *);
		plist.arg[0] = NULL;				/* First arg is extended reference that simpleAPI doesn't support */
		varname_mval.str.addr = varname->buf_addr;	/* Second arg is varname */
		varname_mval.str.len = varname->len_used;
		plist.arg[1] = &varname_mval;
		/* Note, no rebuffering is needed here as the lock name and subscripts are all copied into the private
		 * lock block space by routines called by op_lkname so are effectively already rebuffered. No need for
		 * us to do it again.
		 */
		COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 2, "ydb_lock_s()");
		callg((callgfnptr)op_lkname, &plist);
	}
	va_end(var);
	/* At this point, all of the private lock blocks have been created. Remaining task before calling "op_lock2" is to
	 * convert the timeout value from microseconds to seconds.
	 */
	assert(MAXPOSINT4 >= (timeout_nsec / NANOSECS_IN_MSEC));	/* Or else a TIME2LONG error would have been issued above */
	timeout_sec = (timeout_nsec / NANOSECS_IN_SEC);
	i2mval(&timeout_mval, (int)timeout_sec);
	/* The generated code typically calls "op_lock" but that routine just calls "op_lock2" */
	lock_rc = op_lock2(&timeout_mval, CM_LOCKS);
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return lock_rc ? YDB_OK : YDB_LOCK_TIMEOUT;
}
