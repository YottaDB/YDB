/****************************************************************
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
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
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	LIBYOTTADB_INIT_NOSIMPLEAPINEST_CHECK((int));	/* Avoid SIMPLEAPINEST error check */
	/* "ydb_ci_exec" already sets up a condition handler "gtmci_ch" so we do not do an
	 * ESTABLISH_RET of ydb_simpleapi_ch here like is done for other SimpleAPI function calls.
	 */
	VAR_START(var, ci_info);
	retval = ydb_ci_exec(ci_info->rtn_name.address, ci_info, var, FALSE); /* Note: "va_end(var)" done inside "ydb_ci_exec" */
	return retval;
}
