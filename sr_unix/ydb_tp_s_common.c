/***************************************************************
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries. *
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
#include "deferred_events_queue.h"
#include "namelook.h"

GBLREF	volatile int4	outofband;
GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int	t_tries;
GBLREF	int		tprestart_state;
GBLREF	uint4		dollar_trestart;
GBLREF	uint4		simpleapi_dollar_trestart;
GBLREF	boolean_t	noThreadAPI_active;
GBLREF	boolean_t	exit_handler_active;
#ifdef DEBUG
GBLREF	uint4		dollar_tlevel;
#endif

/* Routine to invoke a user-specified function "tpfn" inside a TP transaction (i.e. TSTART/TCOMMIT fence).
 *
 * Parameters:
 *   lydbrtn     - Can be LYDB_RTN_TP (SimpleAPI) or LYDB_RTN_TP_START (SimpleThreadAPI) or LYDB_RTN_TP_COMMIT (SimpleThreadAPI)
 *			OR their TLVL0 equivalents (i.e. LYDB_RTN_TP_TLVL0, LYDB_RTN_TP_START_TLVL0, LYDB_RTN_TP_COMMIT_TLVL0
 *							or LYDB_RTN_TP_RESTART_TLVL0 or LYDB_RTN_TP_ROLLBACK_TLVL0)
 *   transid     - Transaction id.
 *   varnamelist - Comma-separated list of variable names that are preserved across restarts in this TP transaction.
 *   tpfn        - Pointer to a function that executes user-specified code inside of the TSTART/TCOMMIT fence.
 *   tpfnparm    - Parameter that is passed to the user C function "tpfn". Can be NULL if not needed.
 */
int ydb_tp_s_common(libyottadb_routines lydbrtn,
			ydb_basicfnptr_t tpfn, void *tpfnparm, const char *transid, int namecount, const ydb_buffer_t *varnames)
{
	boolean_t		error_encountered;
	mval			tid;
	int			rc, tpfn_status, tstart_flag;
	mval			varnamearray[YDB_MAX_NAMES], *mv, *mv_top;
	const ydb_buffer_t	*curvarname;
	int			nested_tp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(LYDB_RTN_TP < LYDB_RTN_TP_START);
	assert(LYDB_RTN_TP_START < LYDB_RTN_TP_COMMIT);
	assert(LYDB_RTN_TP_COMMIT < LYDB_RTN_TP_RESTART);
	assert(LYDB_RTN_TP_RESTART < LYDB_RTN_TP_ROLLBACK);
	assert(LYDB_RTN_TP_ROLLBACK < LYDB_RTN_TP_TLVL0);
	assert((LYDB_RTN_TP_TLVL0 - LYDB_RTN_TP) == (LYDB_RTN_TP_START_TLVL0 - LYDB_RTN_TP_START));
	assert((LYDB_RTN_TP_TLVL0 - LYDB_RTN_TP) == (LYDB_RTN_TP_COMMIT_TLVL0 - LYDB_RTN_TP_COMMIT));
	assert((LYDB_RTN_TP_TLVL0 - LYDB_RTN_TP) == (LYDB_RTN_TP_RESTART_TLVL0 - LYDB_RTN_TP_RESTART));
	assert((LYDB_RTN_TP_TLVL0 - LYDB_RTN_TP) == (LYDB_RTN_TP_ROLLBACK_TLVL0 - LYDB_RTN_TP_ROLLBACK));
	/* Set "nested_tp" to 0 in case lydbrtn is any of the _TLVL0 codes else set it to a non-zero value.
	 * 0 value indicates this is the outermost TP else it is a nested TP (used below for restart processing).
	 */
	if (LYDB_RTN_TP_TLVL0 > lydbrtn)
	{
		nested_tp = TRUE;
		assert(LYDB_RTN_TP_RESTART != lydbrtn);		/* Callers ensure this value is never passed */
	} else
	{
		nested_tp = FALSE;
		/* Now that "nested_tp" has been determined, revert "lydbrtn" to its non-_TLVL0 counterpart for ease of use below */
		lydbrtn = lydbrtn - (LYDB_RTN_TP_TLVL0 - LYDB_RTN_TP);
	}
	assert((LYDB_RTN_TP == lydbrtn)
		|| (LYDB_RTN_TP_START == lydbrtn) || (LYDB_RTN_TP_COMMIT == lydbrtn)
		|| (LYDB_RTN_TP_RESTART == lydbrtn) || (LYDB_RTN_TP_ROLLBACK == lydbrtn));
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(lydbrtn, (int));		/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	if (LYDB_RTN_TP_START >= lydbrtn)
	{	/* LYDB_RTN_TP (SimpleAPI) or LYDB_RTN_TP_START (SimpleThreadAPI) */
		assert((LYDB_RTN_TP == lydbrtn) || (LYDB_RTN_TP_START == lydbrtn));
		ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
		if (error_encountered)
		{
			assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
			REVERT;
			assert(ERR_TPRETRY != SIGNAL);
			return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
		}
		/* Check if an outofband action that might care about has popped up */
		if (outofband)
			outofband_action(FALSE);
		if (0 > namecount)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVNAMECOUNT, 2, LEN_AND_STR(LYDBRTNNAME(lydbrtn)));
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
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_NAMECOUNT2HI, 4,
					LEN_AND_STR(LYDBRTNNAME(lydbrtn)), namecount, YDB_MAX_NAMES);
			for (curvarname = varnames, mv = varnamearray, mv_top = mv + namecount; mv < mv_top; curvarname++, mv++)
			{
				ydb_var_types	get_type;
				int		get_svn_index, index;

				index = curvarname - varnames;
				/* In this case, we are guaranteed to be looking at unsubscripted local variable names so
				 * pass in "0" as the "subs_used" parameter (2nd parameter) to the macro call below.
				 */
				VALIDATE_VARNAME(curvarname, 0, FALSE, lydbrtn, index, get_type, get_svn_index);
				/* Note the variable name is put in the stringpool. Needed if this variable does not yet exist in
				 * the current symbol table, a pointer to this string is added in op_tstart as part of the
				 * "add_hashtab_mname_symval" call and that would then point to memory in user-driven C program
				 * which cannot be assumed to be stable for the rest of the lifetime of this process.
				 */
				mv->mvtype = MV_STR;
				mv->str.addr = curvarname->buf_addr;
				mv->str.len = curvarname->len_used;
				s2pool(&mv->str);
				RECORD_MSTR_FOR_GC(&mv->str);
			}
			/* Now that no errors are detected, pass this array of mvals (containing the names of variables to be
			 * preserved) to "op_tstart" with a special value (LISTLOCAL) so it knows this format and parses this
			 * differently from the usual "op_tstart" invocations (where each variable name is a separate mval
			 * pointer in a var-args list).
			 */
			op_tstart(tstart_flag, TRUE, &tid, LISTLOCAL, namecount, varnamearray);
			TREF(sapi_mstrs_for_gc_indx) = 0;
					/* mstrs in this array (added by RECORD_MSTR_FOR_GC) no longer need protection */
		}
		assert(dollar_tlevel);
		/* Now that op_tstart has been done, revert our previous handler (which had an error return up top) in favor of
		 * re-establishing the handler so it returns here so we can deal with errors within the transaction differently.
		 */
		REVERT;
		if (LYDB_RTN_TP_START == lydbrtn)
		{	/* The "op_tstart" part of the SimpleThreadAPI TP transaction is done. Return now. */
			LIBYOTTADB_DONE;		/* Shutoff active rtn indicator while TP callback routine is driven */
			return YDB_OK;
		}
	}
	tpfn_status = YDB_OK;
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{	/* If we reach here, it means an error occurred and "ydb_simpleapi_ch" was invoked which did a "longjmp"/"UNWIND".
		 * Note: tpfn_status should at all times be negative since that is the final return value of "ydb_tp_s".
		 * But TREF(ydb_error_code) is the error code currently encountered and is positive hence the negation below.
		 */
		tpfn_status = -TREF(ydb_error_code);
		assert(0 > tpfn_status);
		assert(-ERR_TPRETRY == YDB_ERR_TPRETRY);
		if (YDB_ERR_TPRETRY == tpfn_status)
 		{
			assert(dollar_tlevel);	/* ensure "dollar_tlevel" is still non-zero */
			/* If we reach here, it means we have a TPRETRY error from the database engine */
			if (nested_tp || (LYDB_RTN_TP_COMMIT == lydbrtn))
			{	/* If "nested_tp" is TRUE, we were already inside a transaction when we entered this
				 *   "ydb_tp_s" invocation and so pass the TPRETRY to the caller until we go back to the
				 *   outermost "ydb_tp_s" invocation.
				 * If "lydbrtn" is LYDB_RTN_TP_COMMIT and we reach here, it means we got a TPRETRY while
				 *   inside "op_tcommit" so we need to return with the restart code to "ydb_tp_st"
				 *   so it can then trigger restart processing and then reinvoke the user-defined callback
				 *   function before attempting another "op_tcommit".
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
			if (LYDB_RTN_TP_RESTART == lydbrtn)
			{	/* Now that restart processing has happened, return control to "ydb_tp_st" to invoke
				 * the user-defined callback function.
				 */
				LIBYOTTADB_DONE;
				REVERT;
				assert(dollar_tlevel);
				return YDB_OK;
			}
		}
	}
	switch(lydbrtn)
	{
	case LYDB_RTN_TP:
		if (!nested_tp)
		{	/* This is the outermost TP. Note down "simpleapi_dollar_trestart" for later use
			 * by "ydb_tp_s"/"ydb_simpleapi_ch" to help decide whether "tp_restart" processing is needed.
			 */
			simpleapi_dollar_trestart = dollar_trestart;
		}
		/* SimpleAPI */
		if (YDB_OK == tpfn_status)
		{
			LIBYOTTADB_DONE;		/* Shutoff active rtn indicator while TP callback routine is driven */
			/* Drive the user-specified TP callback routine */
			tpfn_status = (*tpfn)(tpfnparm);
			if (exit_handler_active)
			{	/* Note if the exit handler ran during the TP callback routine, we potentially have an active
				 * routine again so clear it again before we leave.
				 */
				LIBYOTTADB_DONE;	/* If there was no active routine, this macro has no effect */
				REVERT;
				return YDB_ERR_CALLINAFTERXIT;
			}
			assert(dollar_tlevel);			/* ensure "dollar_tlevel" is still non-zero */
			TREF(libyottadb_active_rtn) = LYDB_RTN_TP; /* Restore our routine indicator (i.e. redo LIBYOTTADB_INIT) */
		}
		break;
	case LYDB_RTN_TP_RESTART:
		assert(!nested_tp);
		assert(YDB_OK == tpfn_status);
		/* Set "tpfn_status" to YDB_TP_RESTART so we fall through to the "INVOKE_RESTART" code block below
		 * which will in turn transfer control to the "if (error_encountered)" block above and trigger "tp_restart"
		 * processing as needed.
		 */
		tpfn_status = YDB_TP_RESTART;
		break;
	case LYDB_RTN_TP_ROLLBACK:
		assert(YDB_OK == tpfn_status);
		/* Set "tpfn_status" so we fall through to the OP_TROLLBACK below */
		tpfn_status = YDB_TP_ROLLBACK;
		break;
	default:
		break;
	}
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
		if (!nested_tp)
		{	/* This is the outermost "ydb_tp_s" invocation. Rollback the transaction and pass the error to user code.
			 * See comment above about "preemptive_db_clnup" for why it is needed.
			 */
			preemptive_db_clnup(ERROR);	/* Cleanup "reset_gv_target", TREF(expand_prev_key) etc. */
			OP_TROLLBACK(0);
			assert(!dollar_tlevel);
		} else
		{	/* We were already inside a transaction when we entered this "ydb_tp_s" invocation.
			 * So bubble the error back to the caller until we go back to the outermost "ydb_tp_s" invocation.
			 * But before that, rollback the current TP level if returning a non-zero error code.
			 */
			OP_TROLLBACK(-1);
		}
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* The counter should have been reset in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return tpfn_status;
}
