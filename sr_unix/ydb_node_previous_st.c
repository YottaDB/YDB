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

/* Routine to drive ydb_node_previous_s() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_node_previous_s(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_node_previous_s()
 * still so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_node_previous_s() except for the addition of tptoken and errstr.
 */
int ydb_node_previous_st(uint64_t tptoken, ydb_buffer_t *errstr, ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray,
				int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	intptr_t retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int));
	VERIFY_THREADED_API((int));
	retval = ydb_stm_args5(tptoken, errstr, LYDB_RTN_NODE_PREVIOUS, (uintptr_t)varname, (uintptr_t)subs_used,
			       (uintptr_t)subsarray, (uintptr_t)ret_subs_used, (uintptr_t)ret_subsarray);
	return (int)retval;
}
