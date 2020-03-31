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

#include <stdarg.h>
#include "gtm_string.h"

#include "libyottadb_int.h"
#include "error.h"
#include "gtmci.h"

GBLREF	boolean_t	caller_func_is_stapi;

/* Routine to return information about the specified routine name from the current call-in table. If the routine
 * is not found in the current call-in table, return YDB_ERR_CINOENTRY.
 *
 * Parameters:
 *   tptoken - TP token for this TP level.
 *   errstr  - Buffer for error message unique to this thread.
 *   rtnname - the name of the routine to lookup and return info on.
 *   pptype  - the filled in information block returned to the program.
 *
 * Note, the API for calling M routines from languages other than C is evolving so this routine should NOT
 * be part of the documented API as it is highly likely to change significantly in future releases.
 *
 * Note this version is the *threaded* version
 */
int ydb_ci_get_info_t(uint64_t tptoken, ydb_buffer_t *errstr, const char *rtnname, ci_parm_type *pptype)
{
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	int			retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	threaded_api_ydb_engine_lock(tptoken, errstr, LYDB_RTN_YDB_CI_GET_INFO, &save_active_stapi_rtn, &save_errstr, &get_lock,
				     &retval);
	if (YDB_OK == retval)
	{
		caller_func_is_stapi = TRUE;	/* used to inform below SimpleAPI call that caller is SimpleThreadAPI */
		retval = ydb_ci_get_info(rtnname, pptype);
		threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr, get_lock);
	}
	return retval;
}
