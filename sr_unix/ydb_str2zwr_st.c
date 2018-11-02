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

#include "libyottadb_int.h"

/* Routine to drive ydb_str2zwr_s() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_str2zwr_s(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_str2zwr_s() still
 * so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_str2zwr_s() except for the addition of tptoken.
 */
int ydb_str2zwr_st(uint64_t tptoken, ydb_buffer_t *str, ydb_buffer_t *zwr)
{
	intptr_t retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int));
	VERIFY_THREADED_API((int));
	retval = ydb_stm_args2(tptoken, LYDB_RTN_STR2ZWR, (uintptr_t)str, (uintptr_t)zwr);
	return (int)retval;
}
