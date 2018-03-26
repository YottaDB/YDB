/****************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
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

/* Note: Some code in this routine was taken from gtm_filename_to id() hence the inclusion of the
 * FIS copyright but this module was not written by FIS.
 *
 * Because this is a re-implementation of gtm_filename_to_id(), any changes there need to be
 * reflected here.
 */

#include "mdef.h"

#include "gtmxc_types.h"
#include "error.h"
#include "send_msg.h"
#include "libydberrors.h"
#include "gdsroot.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "is_file_identical.h"
#include "libyottadb.h"

/* YottaDB reimplementation of the gtm_filename_to_id() utility function that allows the return
 * of an error code if it fails.
 */
int ydb_file_name_to_id(ydb_string_t *filename, ydb_fileid_ptr_t *fileid)
{
	boolean_t	error_encountered;
	ydb_status_t	status;
	gd_id_ptr_t	tmp_fileid;
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
	if (!filename)
	{
		REVERT;
		return YDB_NOTOK;
	}
	assert(fileid && !*fileid);
	tmp_fileid = (gd_id_ptr_t)malloc(SIZEOF(gd_id));
	status = filename_to_id(tmp_fileid, filename->address);
	if (SS_NORMAL == status)
	{
		*fileid = (ydb_fileid_ptr_t)tmp_fileid;
		status = YDB_OK;
	} else
	{	/* There was an error */
		free(tmp_fileid);
		*fileid = NULL;
	}
	REVERT;
	return (SS_NORMAL == status) ? YDB_OK : -status;
}
