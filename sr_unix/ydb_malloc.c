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

#include "gtmxc_types.h"
#include "libyottadb_int.h"
#include "error.h"
#include "send_msg.h"
#include "libydberrors.h"

/* Simple YottaDB wrapper for gtm_malloc() */
void *ydb_malloc(size_t size)
{
	void 		*storaddr;
	boolean_t	error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_INIT(LYDB_RTN_MALLOC, (void *)(uintptr_t));	/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));		/* Previously unused entries should have been cleared by that
								 * corresponding ydb_*_s() call.
								 */
	VERIFY_NON_THREADED_API_RETNULL;
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - return NULL to the caller ($ZSTATUS is set) */
		REVERT;
		return NULL;
	}
	storaddr = gtm_malloc(size);
	LIBYOTTADB_DONE;
	REVERT;
	return storaddr;
}
