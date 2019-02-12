/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
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

/* Routine to drive ydb_call_variadic_plist_func_s() in a worker thread so YottaDB access is isolated. Note because this
 * drives an underlying SimpleAPI call, we don't do any of the exclusive access checks here. The thread management
 * itself takes care of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in
 * the underlying SimpleAPI call still so no need for it here. The one exception to this is that we need to make sure
 * the run time is alive.
 *
 * Parms and return - same as ydb_call_variadic_plist_func_s() except for the addition of tptoken and errstr.
 */
int ydb_call_variadic_plist_func_st(uint64_t tptoken, ydb_buffer_t *errstr, ydb_vplist_func cgfunc, uintptr_t cvplist)
{
	intptr_t retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	THREADED_API_YDB_ENGINE_LOCK(tptoken, errstr);
	retval = ydb_call_variadic_plist_func_s(cgfunc, cvplist);
	THREADED_API_YDB_ENGINE_UNLOCK(tptoken, errstr);
	return (int)retval;
}
