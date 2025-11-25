/****************************************************************
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
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
#include "error_trap.h"
#include "gtmci.h"

/* Routine to drive ydb_ci() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_ci(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_ci()
 * still so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_ci() except for the addition of tptoken and errstr.
 */
int ydb_ci_t(uint64_t tptoken, ydb_buffer_t *errstr, const char *c_rtn_name, ...)
{
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	va_list			var;
	int			retval;
	ci_name_descriptor	ci_desc;
	DCL_THREADGBL_ACCESS;

	LIBYOTTADB_RUNTIME_CHECK((int), errstr);	/* Note: Also does SETUP_THREADGBL_ACCESS; May return if error */
	VERIFY_THREADED_API((int), errstr);
	VAR_START(var, c_rtn_name);
	/* Ready a call-in name descriptor so we can use "ydb_cip_helper" */
	ci_desc.rtn_name.address = (char *)c_rtn_name;
	ci_desc.rtn_name.length = STRLEN(ci_desc.rtn_name.address);
	ci_desc.handle = NULL;
	threaded_api_ydb_engine_lock(tptoken, errstr, LYDB_RTN_YDB_CI, &save_active_stapi_rtn, &save_errstr, &get_lock, &retval);
	if (YDB_OK == retval)
	{
		/* We want to start and end ydb_ci_t in a clean, non-error state.
		 * To get that, we clear dollar_ecode BEFORE and AFTER the ydb_cip_helper() function call.
		 */
		NULLIFY_DOLLAR_ECODE;
		retval = ydb_cip_helper(LYDB_RTN_YDB_CI, &ci_desc, &var); /* Note: "va_end(var)" done inside "ydb_cip_helper()" */
		if (0 != retval)
			/* Set errstr from $zstatus before releasing the YottaDB multi-threaded engine lock */
			SET_ERRSTR_FROM_ZSTATUS_IF_NOT_NULL(errstr);
		NULLIFY_DOLLAR_ECODE;
		threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr, get_lock);
	} else
		va_end(var);
	return retval;
}
