/****************************************************************
 *								*
 * Copyright (c) 2019-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "libyottadb_int.h"

GBLREF	boolean_t	caller_func_is_stapi;

/* Routine to drive ydb_encode_s() in a worker thread so YottaDB access is isolated. Note because this drives ydb_encode_s(),
 * we don't do any of the exclusive access checks here. The thread management itself takes care of most of that currently
 * but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_encode_s() still so no need for it here. The one
 * exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_encode_s() except for the addition of tptoken and errstr.
 */
int ydb_encode_st(uint64_t tptoken, ydb_buffer_t *errstr, const ydb_buffer_t *varname, int subs_used,
				const ydb_buffer_t *subsarray, const char *format, ydb_buffer_t *ret_value)
{
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	int			retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	threaded_api_ydb_engine_lock(tptoken, errstr, LYDB_RTN_ENCODE, &save_active_stapi_rtn, &save_errstr, &get_lock, &retval);
	if (YDB_OK == retval)
	{
		caller_func_is_stapi = TRUE;	/* used to inform below SimpleAPI call that caller is SimpleThreadAPI */
		retval = ydb_encode_s(varname, subs_used, subsarray, format, ret_value);
		threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr, get_lock);
	}
	return (int)retval;
}
