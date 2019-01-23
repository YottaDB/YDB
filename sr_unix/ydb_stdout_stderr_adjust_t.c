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

#include "libyottadb_int.h"

/* Routine to drive ydb_stdout_stderr_adjust_t() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_stdout_stderr_adjust_t(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_stdout_stderr_adjust_t() still
 * so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_delete_s() except for the addition of tptoken and errstr.
 */
int ydb_stdout_stderr_adjust_t(uint64_t tptoken, ydb_buffer_t *errstr)
{
	intptr_t retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	retval = ydb_stm_args0(tptoken, errstr, LYDB_RTN_STDIO_ADJUST);
	return (int)retval;
}
