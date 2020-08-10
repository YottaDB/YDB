/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
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

/* Routine invoked by a SimpleAPI application whenever it notices an EINTR return from a system/library call.
 * Will trigger deferred signal handling in YDB (e.g. exit process in case of a fatal SIGTERM/SIGINT signal etc.) as needed.
 */
int ydb_eintr_handler(void)
{
	boolean_t	error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_EINTR_HANDLER, (int));	/* Note: macro could "return" from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		REVERT;
		return -(TREF(ydb_error_code));
	}
	eintr_handling_check();	/* Handle EINTR */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
