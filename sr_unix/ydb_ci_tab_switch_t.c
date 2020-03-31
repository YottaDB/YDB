/****************************************************************
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
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

GBLREF	boolean_t	caller_func_is_stapi;

/* SimpleThreadAPI equivalent of "ydb_ci_tab_switch".
 * Parms and return - same as "ydb_ci_tab_switch" except for the addition of tptoken and errstr.
 */
int ydb_ci_tab_switch_t(uint64_t tptoken, ydb_buffer_t *errstr, uintptr_t new_handle, uintptr_t *ret_old_handle)
{
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	int			retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	threaded_api_ydb_engine_lock(tptoken, errstr, LYDB_RTN_YDB_CI_TAB_OPEN, &save_active_stapi_rtn,	&save_errstr, &get_lock,
				     &retval);
	if (YDB_OK == retval)
	{
		caller_func_is_stapi = TRUE;	/* used to inform below SimpleAPI call that caller is SimpleThreadAPI */
		retval = ydb_ci_tab_switch(new_handle, ret_old_handle);
		threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr, get_lock);
	}
	return retval;
}
