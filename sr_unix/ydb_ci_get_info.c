/****************************************************************
 *								*
 * Copyright (c) 2020-2023 YottaDB LLC and/or its subsidiaries.	*
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
#include "fgncalsp.h"
#include "deferred_events_queue.h"

GBLREF	volatile int4	outofband;

/* Routine to return information about the specified routine name from the current call-in table. If the routine
 * is not found in the current call-in table, return YDB_ERR_CINOENTRY.
 *
 * Parameters:
 *   rtnname - the name of the routine to lookup and return info on.
 *   pptype  - the filled in information block returned to the program.
 *
 * Note, the API for calling M routines from languages other than C is evolving so this routine should NOT
 * be part of the documented API as it is highly likely to change significantly in future releases.
 */
int ydb_ci_get_info(const char *rtnname, ci_parm_type *pptype)
{
	boolean_t		error_encountered;
	ci_tab_entry_t		*ci_tab;
	callin_entry_list	*ci_entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_YDB_CI_GET_INFO, (int)); /* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have never become non-zero and even if it did,
								 * it should have been cleared by "ydb_simpleapi_ch".
								 */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	ci_entry = ci_load_table_rtn_entry(rtnname, &ci_tab);	/* load ci table, locate entry for return name */
	/* Copy fields as appropriate to return block (if one provided). Note that because this routine does its work
	 * anyway even if an output block is not provided, it provides a cheap way to do any number of things from
	 * initializing the YDB environment, initializing the call-ins for a given table, checking for out-of-band
	 * interrupts, to even seeing if a routine exists before it is called.
	 */
	if (NULL != pptype)
	{
		pptype->input_mask = ci_entry->input_mask;
		pptype->output_mask = ci_entry->output_mask;
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
