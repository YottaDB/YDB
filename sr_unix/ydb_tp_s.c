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

#include "callg.h"
#include "op.h"
#include "libydberrors.h"
#include "libyottadb_int.h"
#include "tp_frame.h"
#include "op_tcommit.h"
#include "cdb_sc.h"
#include "tp_restart.h"
#include "preemptive_db_clnup.h"

GBLREF	uint4		dollar_tlevel;

LITREF	mval		literal_batch;

/* Routine to invoke a user-specified function "tpfn" inside a TP transaction (i.e. TSTART/TCOMMIT fence).
 *
 * Parameters:
 *   transid     - Transaction id
 *   varnamelist - Comma-separated list of variable names that are preserved across restarts in this TP transaction
 *   tpfn        - Pointer to a function that executes user-specified code inside of the TSTART/TCOMMIT fence.
 *   tpfn_parm   - Parameter that is passed to the user C function "tpfn". Can be NULL if not needed.
 */
int ydb_tp_s(ydb_buffer_t *transid, ydb_buffer_t *varnamelist, ydb_tpfnptr_t tpfn, void *tpfn_parm)
{
	boolean_t	error_encountered;
	int		rc, save_dollar_tlevel, tpfn_status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_TP);	/* Note: macro could "return" from this function in case of errors */
	save_dollar_tlevel = dollar_tlevel;
	op_tstart(IMPLICIT_TSTART, TRUE, &literal_batch, 0);
	assert(dollar_tlevel);
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* If we reach here, it means an error occurred and "ydb_simpliapi_ch" was invoked which did a "longjmp"/"UNWIND" */
		if (ERR_TPRETRY != SIGNAL)
		{
			REVERT;
			return -(TREF(ydb_error_code));
		}
		assert(dollar_tlevel);	/* ensure "dollar_tlevel" is still non-zero */
		/* If we reach here, it means we have a TPRETRY error from the database engine */
		if (save_dollar_tlevel)
		{	/* We were already inside a transaction when we entered this "ydb_tp_s" invocation.
			 * So pass the TPRETRY to the caller until we go back to the outermost "ydb_tp_s" invocation.
			 */
			REVERT;
			return YDB_TP_RESTART;
		}
		/* This is the outermost "ydb_tp_s" invocation. Do restart processing right here.
		 * The below code is very similar to that in "gvcst_put" for "tp_restart" handling.
		 */
		preemptive_db_clnup(ERROR);	/* Cleanup "reset_gv_target", TREF(expand_prev_key) etc. */
		rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
	}
	tpfn_status = (*tpfn)(tpfn_parm);
	if (YDB_TP_RESTART == tpfn_status)
	{	/* The user function has asked us to do a TP restart. Do it by issuing a TPRETRY error which will
		 * invoke "ydb_simpliapi_ch" and transfer control to the "if (error_encountered)" block above.
		 */
		INVOKE_RESTART;
	} else if (YDB_TP_ROLLBACK == tpfn_status)
	{	/* The user function has asked us to do a TROLLBACK */
		assert(dollar_tlevel);	/* ensure "dollar_tlevel" is still non-zero */
		if (save_dollar_tlevel)
		{	/* We were already inside a transaction when we entered this "ydb_tp_s" invocation.
			 * So pass the TROLLBACK to the caller until we go back to the outermost "ydb_tp_s" invocation.
			 */
			REVERT;
			return YDB_TP_ROLLBACK;
		}
		/* This is the outermost "ydb_tp_s" invocation. Do rollback processing right here. */
		OP_TROLLBACK(0);
		assert(!dollar_tlevel);
		REVERT;
		return YDB_TP_ROLLBACK;
	}
	op_tcommit();
	/* TODO; Check return value of op_tstart */
	/* TODO; Pass TID */
	/* TODO; Pass list of variable names to preserve */
	/* TODO; Check return value of tpfn */
	/* TODO; Check return value of op_tcommit */
	/* TODO; (from updproc.c) : memcpy(tcom_record.jnl_tid, rec->jrec_tcom.jnl_tid, TID_STR_SIZE); */
	REVERT;
	return YDB_OK;
}
