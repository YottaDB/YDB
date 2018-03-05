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

/* Simple YottaDB wrapper for gtm_hiber_start() */
void	ydb_hiber_start(unsigned long long sleep_nsec)
{
	int			timeoutms;
	unsigned long long	timeout_msec;
	boolean_t		error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (process_exiting)
	{	/* YDB runtime environment not setup/available, no driving of errors */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return;
	}
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - just return to the caller ($ZSTATUS is set) */
		REVERT;
		return;
	}
	if (YDB_MAX_TIME < sleep_nsec)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_TIME2LONG, 1, YDB_MAX_TIME);
	timeout_msec = (sleep_nsec / NANOSECS_IN_MSEC);
	assert(MAXPOSINT4 > timeout_msec);      	/* Or else a TIME2LONG error would have been issued above */
	timeoutms = (int)timeout_msec;
	gtm_hiber_start(timeoutms);
	REVERT;
	return;
}
