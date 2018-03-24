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
#include "error.h"
#include "send_msg.h"
#include "libydberrors.h"
#include "libyottadb_int.h"

/* Simple YottaDB wrapper for gtm_start_timer() */
int	ydb_timer_start(int timer_id, unsigned long long limit_nsec, ydb_funcptr_retvoid_t handler, unsigned int hdata_len,
			void *hdata)
{
	int			timeoutms;
	unsigned long long	timeout_msec;
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
	ISSUE_TIME2LONG_ERROR_IF_NEEDED(limit_nsec);
	assert(MAXPOSINT4 >= (limit_nsec / NANOSECS_IN_MSEC));	/* Or else a TIME2LONG error would have been issued above */
	timeout_msec = (limit_nsec / NANOSECS_IN_MSEC);
	timeoutms = (int)timeout_msec;
	gtm_start_timer(timer_id, timeoutms, handler, hdata_len, hdata);
	REVERT;
	return YDB_OK;
}
