/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC. and/or its subsidiaries.*
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
#include "callg.h"

GBLREF	volatile int4	outofband;

/* Routine to drive ydb_lock_s() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_lock_s(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_lock_s()
 * still so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_lock_s() except for the addition of tptoken and errstr.
 */
int ydb_lock_st(uint64_t tptoken, ydb_buffer_t *errstr, unsigned long long timeout_nsec, int namecount, ...)
{
	va_list		var;
	gparam_list	gparms;
	int		parmcnt, maxparmcnt, indx, i, maxallowednamecount;
#	define 		MAXPARMS ARRAYSIZE(gparms.arg)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	parmcnt = 2;			/* First 2 parms are fixed and always present (not counting tptoken since it
					 * is not passed on to ydb_lock_s()).
					 */
	if (0 > namecount)
	{	/* Can't pass this request on if the count is negative */
		SETUP_GENERIC_ERROR_2PARMS(ERR_INVNAMECOUNT, strlen("ydb_lock_st()"), "ydb_lock_st()");
		return YDB_ERR_INVNAMECOUNT;
	}
	NON_GTM64_ONLY(parmcnt++);		/* Using an extra parm due to 32 bit environment so account for the extra */
	maxparmcnt = parmcnt + (3 * namecount);
	/* Note: It is possible that "namecount" is a positive number when treated as an "int" but (3 * namecount) is a
	 * negative number when treated as an "int". In that case, checking for "MAXPARMS <= maxparmcnt" is not enough
	 * since that will incorrectly fail. Hence the need for "MAXPARMS <= namecount" too.
	 */
	if ((MAXPARMS <= namecount) || (MAXPARMS <= maxparmcnt))
	{	/* Too many parms for this call */
		maxallowednamecount = (int)((MAXPARMS - parmcnt) / 3);
		SETUP_GENERIC_ERROR_3PARMS(ERR_NAMECOUNT2HI, strlen("ydb_lock_st()"), "ydb_lock_st()", maxallowednamecount);
		return YDB_ERR_NAMECOUNT2HI;
	}
	indx = 0;
#	ifdef GTM64
	gparms.arg[indx++] = (void *)timeout_nsec;
#	else /* 32 bit - need to split 8 byte timeout value across 2 parameters */
#	ifdef BIGENDIAN
	gparms.arg[indx++] = (void *)(uintptr_t)(timeout_nsec >> 32);
	gparms.arg[indx++] = (void *)(uintptr_t)(timeout_nsec & 0xffffffff);
#	else
	gparms.arg[indx++] = (void *)(uintptr_t)(timeout_nsec & 0xffffffff);
	gparms.arg[indx++] = (void *)(uintptr_t)(timeout_nsec >> 32);
#	endif
#	endif
	gparms.arg[indx++] = (void *)(uintptr_t)namecount;
	VAR_START(var, namecount);
	for (i = namecount; 0 < i; i--)			/* Once through for each set of 3 parms that make up a lock name */
	{	/* Fetch and store the 3 parms that make up this subscript */
		gparms.arg[indx++] = (void *)(va_arg(var, ydb_buffer_t *));	/* Varname */
		gparms.arg[indx++] = (void *)(uintptr_t)(va_arg(var, int));	/* Subscript count */
		gparms.arg[indx++] = (void *)(va_arg(var, ydb_buffer_t *));	/* Subscript array */
	}
	gparms.n = maxparmcnt;
	/* Have now loaded the callg_nc buffer with the parameters but we can't drive callg() here. We have to
	 * put this request on a queue for execution in the main execution thread.
	 */
	return ydb_call_variadic_plist_func_st(tptoken, errstr, (ydb_vplist_func)&ydb_lock_s, (uintptr_t)&gparms);
}
