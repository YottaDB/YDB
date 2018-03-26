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

/* Simple YottaDB wrapper for the gtm_is_file_identical() utility function */
int ydb_file_is_identical(ydb_fileid_ptr_t fileid1, ydb_fileid_ptr_t fileid2)
{
	boolean_t	error_encountered;
	int		status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (process_exiting)
	{	/* YDB runtime environment not setup/available, no driving of errors */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return YDB_ERR_CALLINAFTERXIT;
	}
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* Some error occurred - return the error code to the caller ($ZSTATUS is set) */
		REVERT;
		return -(TREF(ydb_error_code));
	}
	status = gtm_is_file_identical(fileid1, fileid2);
	REVERT;
	return status ? YDB_OK : YDB_NOTOK;
}
