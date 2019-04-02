/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
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
#include "lv_val.h"
#include "fgncal.h"

/* Opens the call-in table specified in "fname". And returns a handle to the opened table in "ret_value".
 * Returns YDB_OK if call is successful. And a negative number (error) otherwise.
 */
int ydb_ci_tab_open(char *fname, uintptr_t *ret_handle)
{
	boolean_t	error_encountered;
	ci_tab_entry_t	*ci_tab;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	LIBYOTTADB_INIT(LYDB_RTN_YDB_CI_TAB_OPEN, (int));	/* Note: macro could return from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - return the error code to the caller ($ZSTATUS is set) */
		REVERT;
		return -(TREF(ydb_error_code));
	}
	if (NULL == fname)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL fname"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_YDB_CI_TAB_OPEN)));
	if (NULL == ret_handle)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_handle"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_YDB_CI_TAB_OPEN)));
	ci_tab = ci_tab_entry_open(INTERNAL_USE_FALSE, fname);
	*ret_handle = (uintptr_t)ci_tab;
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
