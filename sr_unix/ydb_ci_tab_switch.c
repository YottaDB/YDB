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

/* Switches the current call-in to the handle pointed to by "new_handle" (would have been returned as "ret_handle" in a prior
 * "ydb_ci_tab_open" call). And returns the current call-in handle before the switch in "ret_old_handle".
 * Returns YDB_OK if call is successful. And a negative number (error) otherwise.
 */
int ydb_ci_tab_switch(uintptr_t new_handle, uintptr_t *ret_old_handle)
{
	boolean_t	error_encountered;
	ci_tab_entry_t	*ci_tab, *new_tab, *old_tab;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	LIBYOTTADB_INIT(LYDB_RTN_YDB_CI_TAB_SWITCH, (int));	/* Note: macro could return from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - return the error code to the caller ($ZSTATUS is set) */
		REVERT;
		return -(TREF(ydb_error_code));
	}
	if (NULL == ret_old_handle)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_old_handle"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_YDB_CI_TAB_SWITCH)));
	new_tab = (ci_tab_entry_t *)new_handle;
	/* Verify that input "new_handle" is a valid call-in table handle */
	for (ci_tab = TREF(ci_table_all); NULL != ci_tab; ci_tab = ci_tab->next)
	{
		if (ci_tab == new_tab)
			break;
	}
	if ((NULL != new_tab) && (NULL == ci_tab))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("Invalid new_handle"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_YDB_CI_TAB_SWITCH)));
	old_tab = TREF(ci_table_curr);
	assert((NULL == old_tab) || (TREF(ci_table_internal_filter) != old_tab));
	assert((NULL == ci_tab) || (TREF(ci_table_internal_filter) != ci_tab));
	TREF(ci_table_curr) = ci_tab;
	*ret_old_handle = (uintptr_t)old_tab;
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
