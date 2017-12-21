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

#include "callg.h"
#include "op.h"
#include "libydberrors.h"
#include "libyottadb_int.h"
#include "tp_frame.h"
#include "op_tcommit.h"
#include "cdb_sc.h"
#include "gdsroot.h"
#include "tp_restart.h"
#include "preemptive_db_clnup.h"
#include "stringpool.h"

GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int	t_tries;

/* Routine to invoke a user-specified function "tpfn" inside a TP transaction (i.e. TSTART/TCOMMIT fence).
 *
 * Parameters:
 *   transid     - Transaction id
 *   varnamelist - Comma-separated list of variable names that are preserved across restarts in this TP transaction
 *   tpfn        - Pointer to a function that executes user-specified code inside of the TSTART/TCOMMIT fence.
 *   tpfnparm   - Parameter that is passed to the user C function "tpfn". Can be NULL if not needed.
 */
int ydb_tp_s(ydb_tpfnptr_t tpfn, void *tpfnparm, const char *transid, const char *varnamelist)
{
	boolean_t	error_encountered, done;
	char		*ptr, *ptr_top, *ptr_start;
	mval		tid;
	int		rc, save_dollar_tlevel, tpfn_status, varnamelist_len, ptr_len, tstart_flag;
	mval		varnamearray[LISTLOCAL_MAXNAMES], *mv;
	int		varnamearray_len = 0;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_TP);	/* Note: macro could "return" from this function in case of errors */
	save_dollar_tlevel = dollar_tlevel;
	/* Ready "transid" for passing to "op_tstart" */
	tid.mvtype = MV_STR;
	if (NULL == transid)
		tid.str.len = 0;
	else
	{
		tid.str.len = STRLEN(transid);
		tid.str.addr = (char *)transid;
	}
	/* Ready "varnamelist" for passing to "op_tstart" */
	tstart_flag = IMPLICIT_TSTART | YDB_TP_S_TSTART;
	if (NULL == varnamelist)
		op_tstart(tstart_flag, TRUE, &tid, 0);
	else if (('*' == varnamelist[0]) && ('\0' == varnamelist[1]))
	{	/* preserve all local variables */
		op_tstart(tstart_flag, TRUE, &tid, ALLLOCAL);
	} else
	{	/* varnamelist is a comma-separated list of variable names that need to be preserved.
		 * First do some error checking on input.
		 */
		varnamelist_len = 0;
		for (ptr = (char *)varnamelist, ptr_top = ptr + STRLEN(varnamelist); ptr < ptr_top; ptr++)
		{
			if (LISTLOCAL_MAXNAMES == varnamelist_len)
			{
				/* NARSTODO: Issue new error where "ydb_tp_s" specifies > 256 variable names to preserve */
			}
			mv = &varnamearray[varnamearray_len++];
			mv->mvtype = MV_STR;
			ptr_start = ptr;
			mv->str.addr = ptr_start;
			done = FALSE;
			do
			{
				if (',' == *ptr)
					break;
				ptr++;
				if (ptr == ptr_top)
				{
					done = TRUE;
					break;
				}
			} while (TRUE);
			mv->str.len = ptr_len = ptr - ptr_start;
			VALIDATE_MNAME_C1(ptr_start, ptr_len);
			/* Note the variable name in the stringpool. Needed if this variable does not yet exist in the
			 * current symbol table, a pointer to this string is added in op_tstart as part of the
			 * "add_hashtab_mname_symval" call and that would then point to memory in user-driven C program
			 * which cannot be assumed to be stable for the rest of the lifetime of this process.
			 */
			s2pool(&mv->str);
			if (!done)
				continue;
			break;
		}
		/* Now that no errors are detected, pass this array of mvals (containing the names of variables to be preserved)
		 * to "op_tstart" with a special value (LISTLOCAL) so it knows this format and parses this differently from the
		 * usual "op_tstart" invocations (where each variable name is a separate mval pointer in a var-args list).
		 */
		op_tstart(tstart_flag, TRUE, &tid, LISTLOCAL, varnamearray_len, varnamearray);
	}
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
			if (cdb_sc_normal == t_fail_hist[t_tries])
			{	/* User-induced TP restart. In that case, simulate TRESTART command in M.
				 * This is relied upon by an assert in "tp_restart".
				 */
				op_trestart_set_cdb_code();
			}
			rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
			tpfn_status = YDB_OK;	/* Now that the restart processing is complete, clear status back to normal */
		}
	}
	if (YDB_OK == tpfn_status)
	{
		tpfn_status = (*tpfn)(tpfnparm);
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
	/* NARSTODO; Pass list of variable names to preserve */
	REVERT;
	return tpfn_status;
}
