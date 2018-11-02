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

#include "gt_timer.h"
#include "error.h"
#include "send_msg.h"
#include "libyottadb_int.h"
#include "libydberrors.h"

GBLREF	boolean_t	simpleThreadAPI_active;

/* Simple YottaDB wrapper for gtm_hiber_start_wait_any() */
int	ydb_hiber_start_wait_any(unsigned long long sleep_nsec)
{
	int			sleepms;
	unsigned long long	sleep_msec, max_time_nsec;
	boolean_t		error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_INIT(LYDB_RTN_HIBER_START_ANY, (int));	/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));		/* Previously unused entries should have been cleared by that
								 * corresponding ydb_*_s() call.
								 */
	VERIFY_NON_THREADED_API;
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - just return to the caller ($ZSTATUS is set) */
		REVERT;
		return -(TREF(ydb_error_code));
	}
	ISSUE_TIME2LONG_ERROR_IF_NEEDED(sleep_nsec);
	if (simpleThreadAPI_active)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOTSUPSTAPI, 2, RTS_ERROR_LITERAL("ydb_hiber_startwaut_any()"));
	assert(MAXPOSINT4 >= (sleep_nsec / NANOSECS_IN_MSEC));	/* Or else a TIME2LONG error would have been issued above */
	sleep_msec = (sleep_nsec / NANOSECS_IN_MSEC);
	sleepms = (int)sleep_msec;
	hiber_start_wait_any(sleepms);
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
