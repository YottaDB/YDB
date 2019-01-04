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

/* Routine to drive ydb_cip() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_cip(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_cip()
 * still so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_cip() except for the addition of tptoken and errstr.
 */
int ydb_cip_t(uint64_t tptoken, ydb_buffer_t *errstr, ci_name_descriptor *ci_info, ...)
{
	va_list		var;
	intptr_t	retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int));
	VERIFY_THREADED_API((int));
	VAR_START(var, ci_info);
	/* Note: "va_end(var)" done inside "ydb_ci_exec" when this gets run in the MAIN worker thread */
	retval = ydb_stm_args2(tptoken, errstr, LYDB_RTN_YDB_CIP, (uintptr_t)ci_info, (uintptr_t)&var);
	return (int)retval;
}
