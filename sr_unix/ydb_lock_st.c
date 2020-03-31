/****************************************************************
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
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

GBLREF	volatile int4	outofband;
GBLREF	boolean_t	caller_func_is_stapi;

/* Routine to drive ydb_lock_s() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_lock_s(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_lock_s()
 * still so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_lock_s() except for the addition of tptoken and errstr.
 */
int ydb_lock_st(uint64_t tptoken, ydb_buffer_t *errstr, unsigned long long timeout_nsec, int namecount, ...)
{
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	int			retval;
	va_list			var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	threaded_api_ydb_engine_lock(tptoken, errstr, LYDB_RTN_LOCK, &save_active_stapi_rtn, &save_errstr, &get_lock, &retval);
	if (YDB_OK == retval)
	{
		VAR_START(var, namecount);
		caller_func_is_stapi = TRUE;	/* used to inform below SimpleAPI call that caller is SimpleThreadAPI */
		retval = ydb_lock_s_va(timeout_nsec, namecount, var);
		threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr, get_lock);
	}
	return retval;
}
