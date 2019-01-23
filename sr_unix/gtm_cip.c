/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC. and/or its subsidiaries.	*
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

#include "gtmxc_types.h"
#include "libyottadb_int.h"
#include "gtmci.h"

#undef gtm_cip		/* Undo the transformation of gtm_cip -> ydb_cip created by gtmxc_types.h */

/* Fast path YottaDB wrapper to do a call-in.
 * Adds a struct parm that contains name resolution info after first call to speed up dispatching.
 *
 * Note this routine is nearly identical to ydb_cip() so any changes in either should be changed in the other. This is done by
 * copying this small routine (which calls a larger common routine) because a simple wrapper doesn't work for a variadic call
 * like this one.
 */
int gtm_cip(ci_name_descriptor* ci_info, ...)
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
	VAR_START(var, ci_info);
	/* Note: "va_end(var)" done inside "ydb_ci_exec" */
	retval = ydb_ci_exec(ci_info->rtn_name.address, ci_info->handle, TRUE, var, FALSE);
	return retval;
}
