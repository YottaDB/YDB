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

/* Simple YottaDB wrapper for the gtm_file_name_to_id() utility function */
int ydb_file_name_to_id(ydb_string_t *filename, ydb_fileid_ptr_t *fileid)
{
	boolean_t	error_encountered;
	ydb_status_t	status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (process_exiting)
	{	/* YDB runtime environment not setup/available, no driving of errors */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return YDB_ERR_CALLINAFTERXIT;
	}
	if (error_encountered)
	{	/* Some error occurred - return the error code to the caller ($ZSTATUS is set) */
		REVERT;
		return -(TREF(ydb_error_code));
	}
	status = gtm_filename_to_id(filename, fileid);
	REVERT;
	return status;
}
