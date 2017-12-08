/****************************************************************
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"

GBLREF	boolean_t	dont_want_core;
GBLREF	boolean_t	created_core;
GBLREF	boolean_t	need_core;

/* Condition handler for simpleAPI environment. This routine catches all errors thrown by the YottaDB engine. The error
 * is basically returned to the user as the negative of the errror to differentiate those errors from positive (success
 * or informative) return codes of this API.
 */
CONDITION_HANDLER(ydb_simpleapi_ch)
{
	mstr		error_loc;

	START_CH(TRUE);
	if ((DUMPABLE) && !SUPPRESS_DUMP)
	{	/* Fatal errors need to create a core dump */
		need_core = TRUE;
		gtm_fork_n_core();
	}
	TREF(sapi_mstrs_for_gc_indx) = 0;		/* These mstrs are no longer protected */
	error_loc.addr = "error at xxx";
	error_loc.len = strlen(error_loc.addr);
	set_zstatus(&error_loc, arg, NULL, FALSE);
	TREF(ydb_error_code) = arg;	/* Record error code for caller */
	UNWIND(NULL, NULL); 		/* Return back to ESTABLISH_NORET() in caller */
}
