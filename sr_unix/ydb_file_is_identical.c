/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "send_msg.h"
#include "libyottadb_int.h"
#include "libydberrors.h"

/* Simple YottaDB wrapper for the gtm_is_file_identical() utility function */
int ydb_file_is_identical(ydb_fileid_ptr_t fileid1, ydb_fileid_ptr_t fileid2)
{
	boolean_t	error_encountered;
	int		status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_INIT(LYDB_RTN_FILE_IS_IDENTICAL, (int));	/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));		/* Previously unused entries should have been cleared by that
								 * corresponding ydb_*_s() call.
								 */
	VERIFY_NON_THREADED_API;
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - return the error code to the caller ($ZSTATUS is set) */
		REVERT;
		return -(TREF(ydb_error_code));
	}
	status = gtm_is_file_identical(fileid1, fileid2);
	LIBYOTTADB_DONE;
	REVERT;
	return status ? YDB_OK : YDB_NOTOK;
}
