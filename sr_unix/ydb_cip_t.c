/****************************************************************
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
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
#include "gtmci.h"

/* Routine to drive ydb_cip() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_cip(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_cip()
 * still so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_cip() except for the addition of tptoken and errstr.
 */
int ydb_cip_t(uint64_t tptoken, ydb_buffer_t *errstr, ci_name_descriptor *ci_info, ...)
{
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	va_list			var;
	int			retval;
	DCL_THREADGBL_ACCESS;

	LIBYOTTADB_RUNTIME_CHECK((int), errstr);	/* Note: Also does SETUP_THREADGBL_ACCESS; May return if error */
	VERIFY_THREADED_API((int), errstr);
	VAR_START(var, ci_info);
	threaded_api_ydb_engine_lock(tptoken, errstr, LYDB_RTN_YDB_CIP, &save_active_stapi_rtn, &save_errstr, &get_lock, &retval);
	if (YDB_OK == retval)
	{
		retval = ydb_cip_helper(LYDB_RTN_YDB_CIP, ci_info, &var); /* Note: "va_end(var)" done inside "ydb_cip_helper()" */
		if (0 != retval)
			/* Set errstr from $zstatus before releasing the YottaDB multi-threaded engine lock */
			SET_ERRSTR_FROM_ZSTATUS_IF_NOT_NULL(errstr);
		threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr, get_lock);
	} else
		va_end(var);
	return retval;
}
