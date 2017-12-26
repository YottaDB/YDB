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
#include "error_trap.h"

#define	ERROR_LOC_LIT	"error at xxx"		/* STEVETODO : come up with some way to signal where the error occurred */

GBLREF	boolean_t		dont_want_core;
GBLREF	boolean_t		created_core;
GBLREF	boolean_t		need_core;
GBLREF  dollar_ecode_type 	dollar_ecode;

/* Condition handler for simpleAPI environment. This routine catches all errors thrown by the YottaDB engine. The error
 * is basically returned to the user as the negative of the error to differentiate those errors from positive (success
 * or informative) return codes of this API.
 */
CONDITION_HANDLER(ydb_simpleapi_ch)
{
	mstr		error_loc;

	START_CH(TRUE);
	if (ERR_REPEATERROR == SIGNAL)
		arg = SIGNAL = dollar_ecode.error_last_ecode;	/* Rethrown error. Get primary error code */
	if ((DUMPABLE) && !SUPPRESS_DUMP)
	{	/* Fatal errors need to create a core dump */
		need_core = TRUE;
		gtm_fork_n_core();
	}
	/* The mstrs that were part of the current ydb_*_s() call and were being protected from "stp_gcol" through a global
	 * array no longer need that protection since we are about to exit from the ydb_*_s() call. So clear the global array index.
	 */
	TREF(sapi_mstrs_for_gc_indx) = 0;
	TREF(sapi_query_node_subs_cnt) = 0;
	error_loc.addr = ERROR_LOC_LIT;
	error_loc.len = STR_LIT_LEN(ERROR_LOC_LIT);
	set_zstatus(&error_loc, arg, NULL, FALSE);
	TREF(ydb_error_code) = arg;	/* Record error code for caller */
	UNWIND(NULL, NULL); 		/* Return back to ESTABLISH_NORET() in caller */
}
