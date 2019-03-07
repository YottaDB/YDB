/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
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

#include "libyottadb_int.h"
#include "gtmci.h"

/* Simple YottaDB wrapper to do a call-in. Does name lookup on each call whereas "ydb_cip" does not
 *
 * Note this routine is nearly identical to gtm_ci() so any changes in either should be changed in the other. This is done by
 * copying this small routine (which calls a larger common routine) because a simple wrapper doesn't work for a variadic call
 * like this one.
 */
int ydb_ci(const char *c_rtn_name, ...)
{
	va_list		var;
	int		retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Do not use LIBYOTTADB_INIT to avoid unnecessary SIMPLEAPINEST errors. Instead call LIBYOTTADB_RUNTIME_CHECK macro. */
	LIBYOTTADB_RUNTIME_CHECK((int), NULL);		/* Note: macro could return from this function in case of errors */
	VERIFY_NON_THREADED_API_DO_NOT_SHUTOFF_ACTIVE_RTN;	/* Need to call this version of VERIFY_NON_THREADED_API macro
								 * since LIBYOTTADB_INIT was not called.
								 */
	/* "ydb_ci_exec" already sets up a condition handler "gtmci_ch" so we do not do an
	 * ESTABLISH_RET of ydb_simpleapi_ch here like is done for other SimpleAPI function calls.
	 */
	VAR_START(var, c_rtn_name);
	/* Note: "va_end(var)" done inside "ydb_ci_exec" */
	retval = ydb_ci_exec(c_rtn_name, NULL, FALSE, var, FALSE);
	return retval;
}
