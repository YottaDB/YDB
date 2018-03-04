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

#include "libyottadb.h"
#include "gtmimagename.h"
#include "cenable.h"
#include "io.h"
#include "send_msg.h"
#include "libydberrors.h"

/* This function initializes the IO devices in preparation for a later call-in invocation.
 * This usually needs to be invoked from a C program using simpleAPI after having manipulated the stdout/stderr
 * file descriptors 1 & 2 (for example, if it has done a "dup2" to redirect those descriptors to
 * a different file). In that case, this function reinitializes the M IO context ($P etc. based on the
 * current 1 & 2 descriptors). This is particularly useful in case a C program that is running the child
 * after a "fork" and "ydb_child_init" call has reset 1 & 2 to point to .mjo and .mje files (whereas the parent
 * process had both 1 & 2 pointing to the same physical file). Not doing the "ydb_stdout_stderr_adjust" in this
 * case will cause any error messages that get displayed in the child process while it does a call-in to show
 * up in the .mjo file (instead of the .mje file).
 */
int	ydb_stdout_stderr_adjust(void)
{
	boolean_t	error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (process_exiting)
	{	/* YDB runtime environment not setup/available, no driving of errors */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return YDB_ERR_CALLINAFTERXIT;
	}
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		REVERT;
		return -(TREF(ydb_error_code));
	}
	/* The below 3 lines are similar to code in "gtm_startup" */
	io_init(IS_MUPIP_IMAGE);
	if (!IS_MUPIP_IMAGE)
		cenable();	/* cenable unless the environment indicates otherwise - 2 steps because this can report errors */
	REVERT;
	return YDB_OK;
}
