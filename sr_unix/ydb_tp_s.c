/***************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
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
#include "libyottadb_int.h"
#include "tp_frame.h"
#include "op_tcommit.h"
#include "cdb_sc.h"
#include "gdsroot.h"
#include "tp_restart.h"
#include "preemptive_db_clnup.h"
#include "stringpool.h"
#include "outofband.h"

GBLREF	volatile int4	outofband;
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int	t_tries;
GBLREF	int		tprestart_state;
GBLREF	uint4		dollar_trestart;
GBLREF	uint4		simpleapi_dollar_trestart;

/* Routine to invoke a user-specified function "tpfn" inside a TP transaction (i.e. TSTART/TCOMMIT fence).
 *
 * Parameters:
 *   transid     - Transaction id
 *   varnamelist - Comma-separated list of variable names that are preserved across restarts in this TP transaction
 *   tpfn        - Pointer to a function that executes user-specified code inside of the TSTART/TCOMMIT fence.
 *   tpfnparm   - Parameter that is passed to the user C function "tpfn". Can be NULL if not needed.
 */
int ydb_tp_s(ydb_tpfnptr_t tpfn, void *tpfnparm, const char *transid, int namecount, ydb_buffer_t *varnames)
{
	boolean_t	error_encountered;
	mval		tid;
	int		rc, save_dollar_tlevel, tpfn_status, tstart_flag;
	mval		varnamearray[YDB_MAX_NAMES], *mv, *mv_top;
	ydb_buffer_t	*curvarname;
	char		buff[256];			/* sprintf() buffer */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_TP);	/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	if (0 > namecount)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVNAMECOUNT, 2, RTS_ERROR_LITERAL("ydb_tp_s()"));
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
	if (0 == namecount)
		op_tstart(tstart_flag, TRUE, &tid, 0);
	else if ((1 == namecount) && ('*' == *varnames->buf_addr))
	{	/* preserve all local variables */
		op_tstart(tstart_flag, TRUE, &tid, ALLLOCAL);
	} else
	{	/* varnames is a pointer to an array of ydb_buffer_t structs that describe each varname.
		 * First do some error checking on input.
		 */
		if (YDB_MAX_NAMES < namecount)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_NAMECOUNT2HI, 3,
								RTS_ERROR_LITERAL("ydb_tp_s()"), YDB_MAX_NAMES);
		for (curvarname = varnames, mv = varnamearray, mv_top = mv + namecount; mv < mv_top; curvarname++, mv++)
		{
			if (IS_INVALID_YDB_BUFF_T(curvarname))
			{
				SPRINTF(buff, "Invalid varname array (index %d)", curvarname - varnames);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					      LEN_AND_STR(buff), LEN_AND_LIT(buff));
			}
			if (YDB_MAX_IDENT < curvarname->len_used)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_VARNAME2LONG, 1, YDB_MAX_IDENT);
			VALIDATE_MNAME_C1(curvarname->buf_addr, curvarname->len_used);
			/* Note the variable name is put in the stringpool. Needed if this variable does not yet exist in the
			 * current symbol table, a pointer to this string is added in op_tstart as part of the
			 * "add_hashtab_mname_symval" call and that would then point to memory in user-driven C program
			 * which cannot be assumed to be stable for the rest of the lifetime of this process.
			 */
			mv->mvtype = MV_STR;
			mv->str.addr = curvarname->buf_addr;
			mv->str.len = curvarname->len_used;
			s2pool(&mv->str);
			RECORD_MSTR_FOR_GC(&mv->str);
		}
		/* Now that no errors are detected, pass this array of mvals (containing the names of variables to be preserved)
		 * to "op_tstart" with a special value (LISTLOCAL) so it knows this format and parses this differently from the
		 * usual "op_tstart" invocations (where each variable name is a separate mval pointer in a var-args list).
		 */
		op_tstart(tstart_flag, TRUE, &tid, LISTLOCAL, namecount, varnamearray);
		TREF(sapi_mstrs_for_gc_indx) = 0; /* mstrs in this array (added by RECORD_MSTR_FOR_GC) no longer need protection */
	}
	assert(dollar_tlevel);
	/* Now that op_tstart has been done, revert our previous handler (which had an error return up top) in favor of
	 * re-establishing the handler so it returns here so we can deal with errors within the transaction differently.
	 */
	REVERT;
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
				LIBYOTTADB_DONE;
				REVERT;
				assert(dollar_tlevel);
				return YDB_TP_RESTART;
			}
			/* This is the outermost "ydb_tp_s" invocation. Do restart processing right here.
			 * But before restarting the transaction cleanup database state of the partially completed transaction
			 * using "preemptive_db_clnup". This is normally done by "mdb_condition_handler" in the case of YottaDB
			 * runtime (since a TPRETRY is also an error) but that is not invoked by the simpleAPI and so we need this.
			 * Note that if a restartable situation is detected inside of a call-in, it is possible the "tp_restart"
			 * call happened right there. In that case, we need to skip the "tp_restart" call here. That is
			 * detected by the "if" check below.
			 */
			assert((simpleapi_dollar_trestart == dollar_trestart)
				|| (simpleapi_dollar_trestart == (dollar_trestart - 1)));
			/* If the global "dollar_trestart" is different from what we noted down at the start of this try/retry
			 * of this transaction ("simpleapi_dollar_trestart"), then it means we are done with "tp_restart" call
			 * (since "dollar_trestart++" is done at the end of "tp_restart"). So use that as the basis of
			 * the check to determine if a "tp_restart" is still needed or not.
			 */
			if (simpleapi_dollar_trestart == dollar_trestart)
			{
				preemptive_db_clnup(ERROR);	/* Cleanup "reset_gv_target", TREF(expand_prev_key) etc. */
				if (cdb_sc_normal == t_fail_hist[t_tries])
				{	/* User-induced TP restart. In that case, simulate TRESTART command in M.
					 * This is relied upon by an assert in "tp_restart".
					 */
					op_trestart_set_cdb_code();
				}
				rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
				assert((0 == rc) && (TPRESTART_STATE_NORMAL == tprestart_state));
			}
			tpfn_status = YDB_OK;	/* Now that the restart processing is complete, clear status back to normal */
		}
	}
	LIBYOTTADB_DONE;		/* Shutoff active rtn indicator while call callback routine */
	if (!save_dollar_tlevel)
	{	/* This is the outermost TP. Note down "simpleapi_dollar_trestart" for later use
		 * by "ydb_tp_s"/"ydb_simpleapi_ch" to help decide whether "tp_restart" processing is needed.
		 */
		simpleapi_dollar_trestart = dollar_trestart;
	}
	if (YDB_OK == tpfn_status)
	{
		tpfn_status = (*tpfn)(tpfnparm);	/* Drive the user-specified transaction routine (C function) here */
		assert(dollar_tlevel);	/* ensure "dollar_tlevel" is still non-zero */
	}
	TREF(libyottadb_active_rtn) = LYDB_RTN_TP;		/* Restore our routine indicator */
	if (YDB_OK == tpfn_status)
		op_tcommit();
	else if (YDB_TP_RESTART == tpfn_status)
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
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* The counter should have been reset in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return tpfn_status;
}
