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

/* Routine to drive ydb_timer_cancel() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_timer_cancel(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_timer_cancel() still
 * so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_delete_s() except for the addition of tptoken and errstr.
 */
void ydb_timer_cancel_t(uint64_t tptoken, ydb_buffer_t *errstr, int timer_id)
{
	intptr_t retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK_NORETVAL(errstr);
	VERIFY_THREADED_API_NORETVAL(errstr);
	(void)ydb_stm_args1(tptoken, errstr, LYDB_RTN_TIMER_CANCEL, (uintptr_t)timer_id);
	return;
}
