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
#include "libyottadb.h"
#include "error.h"
#include "send_msg.h"
#include "libydberrors.h"
#include "libyottadb_int.h"

/* Simple YottaDB wrapper for gtm_hiber_start_wait_any() */
int	ydb_hiber_start_wait_any(unsigned long long sleep_nsec)
{
	int			sleepms;
	unsigned long long	sleep_msec, max_time_nsec;
	boolean_t		error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (process_exiting)
	{	/* YDB runtime environment not setup/available, no driving of errors */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return YDB_OK;
	}
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - just return to the caller ($ZSTATUS is set) */
		REVERT;
		return -(TREF(ydb_error_code));
	}
	ISSUE_TIME2LONG_ERROR_IF_NEEDED(sleep_nsec);
	assert(MAXPOSINT4 >= (sleep_nsec / NANOSECS_IN_MSEC));	/* Or else a TIME2LONG error would have been issued above */
	sleep_msec = (sleep_nsec / NANOSECS_IN_MSEC);
	sleepms = (int)sleep_msec;
	hiber_start_wait_any(sleepms);
	REVERT;
	return YDB_OK;
}
