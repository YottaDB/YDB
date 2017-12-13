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

LITREF	mval		literal_batch;

/* Routine to invoke a user-specified function "tpfn" inside a TP transaction (i.e. TSTART/TCOMMIT fence).
 *
 * Parameters:
 *   transid     - Transaction id
 *   varnamelist - Comma-separated list of variable names that are preserved across restarts in this TP transaction
 *   tpfn        - Pointer to a function that executes user-specified code inside of the TSTART/TCOMMIT fence.
 *   tpfn_parm   - Parameter that is passed to "tpfn".
 */
int ydb_tp_s(ydb_buffer_t *transid, ydb_buffer_t *varnamelist, ydb_tpfnptr_t tpfn, void *tpfn_parm)
{
	boolean_t	error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_TP);	/* Note: macro could "return" from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		REVERT;
		return -(TREF(ydb_error_code));
	}
	op_tstart(IMPLICIT_TSTART, TRUE, &literal_batch, 0);
	(*tpfn)(tpfn_parm);
	op_tcommit();
	/* TODO; Check return value of op_tstart */
	/* TODO; Pass TID */
	/* TODO; Pass list of variable names to preserve */
	/* TODO; Check return value of tpfn */
	/* TODO; Check return value of op_tcommit */
	TREF(sapi_mstrs_for_gc_indx) = 0;		/* These mstrs are no longer protected */
	REVERT;
	return YDB_OK;
}
