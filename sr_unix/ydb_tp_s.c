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
	mval		tid;
	int		rc, save_dollar_tlevel, tpfn_status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_TP);	/* Note: macro could "return" from this function in case of errors */
	save_dollar_tlevel = dollar_tlevel;
	tid.mvtype = MV_STR;
	tid.str.len = transid->len_used;
	tid.str.addr = transid->buf_addr;
	op_tstart(IMPLICIT_TSTART, TRUE, &tid, 0);
	assert(dollar_tlevel);
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	tpfn_status = YDB_OK;
	if (error_encountered)
	{	/* If we reach here, it means an error occurred and "ydb_simpleapi_ch" was invoked which did a "longjmp"/"UNWIND" */
		tpfn_status = TREF(ydb_error_code);
		assert(0 < tpfn_status);
		if (ERR_TPRETRY == tpfn_status)
		{
			assert(dollar_tlevel);	/* ensure "dollar_tlevel" is still non-zero */
			/* If we reach here, it means we have a TPRETRY error from the database engine */
			if (save_dollar_tlevel)
			{	/* We were already inside a transaction when we entered this "ydb_tp_s" invocation.
				 * So pass the TPRETRY to the caller until we go back to the outermost "ydb_tp_s" invocation.
				 */
				REVERT;
				assert(dollar_tlevel);
				return YDB_TP_RESTART;
			}
			/* This is the outermost "ydb_tp_s" invocation. Do restart processing right here.
			 * But before restarting the transaction cleanup database state of the partially completed transaction
			 * using "preemptive_db_clnup". This is normally done by "mdb_condition_handler" in the case of YottaDB
			 * runtime (since a TPRETRY is also an error) but that is not invoked by the simpleAPI and so we need this.
			 */
			preemptive_db_clnup(ERROR);	/* Cleanup "reset_gv_target", TREF(expand_prev_key) etc. */
			rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
			tpfn_status = YDB_OK;	/* Now that the restart processing is complete, clear status back to normal */
		}
	}
	if (YDB_OK == tpfn_status)
	{
		tpfn_status = (*tpfn)(tpfn_parm);
		assert(dollar_tlevel);	/* ensure "dollar_tlevel" is still non-zero */
	}
	if (YDB_OK == tpfn_status)
	{
		op_tcommit();
	} else if (YDB_TP_RESTART == tpfn_status)
	{	/* Need to do an implicit or explicit TP restart. Do it by issuing a TPRETRY error which will
		 * invoke "ydb_simpleapi_ch" and transfer control to the "if (error_encountered)" block above.
		 */
		INVOKE_RESTART;
		assert(FALSE);	/* control should never reach here */
	} else
	{	/* Some other error. Pass the error code back to user code (caller of "ydb_tp_s") */
		assert(dollar_tlevel);	/* ensure "dollar_tlevel" is still non-zero */
		if (!save_dollar_tlevel)
		{	/* This is the outermost "ydb_tp_s" invocation. Rollback the transaction and pass the error to user code.
			 * See comment above about "preemptive_db_clnup" for why it is needed.
			 */
			preemptive_db_clnup(ERROR);	/* Cleanup "reset_gv_target", TREF(expand_prev_key) etc. */
			OP_TROLLBACK(0);
			assert(!dollar_tlevel);
		} else
		{	/* We were already inside a transaction when we entered this "ydb_tp_s" invocation.
			 * So bubble the error back to the caller until we go back to the outermost "ydb_tp_s" invocation.
			 */
		}
	}
	/* NARSTODO; Check return value of op_tstart */
	/* NARSTODO; Pass TID */
	/* NARSTODO; Pass list of variable names to preserve */
	/* NARSTODO; Check return value of tpfn */
	/* NARSTODO; Check return value of op_tcommit */
	/* NARSTODO; (from updproc.c) : memcpy(tcom_record.jnl_tid, rec->jrec_tcom.jnl_tid, TID_STR_SIZE); */
	REVERT;
	assert(dollar_tlevel || (YDB_OK == tpfn_status));
	return tpfn_status;
}
