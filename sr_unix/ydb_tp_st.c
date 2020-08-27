/****************************************************************
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "libyottadb_int.h"

GBLREF	uint4		dollar_tlevel;
GBLREF	uint64_t 	stmTPToken;			/* Counter used to generate unique token for SimpleThreadAPI TP */
GBLREF	uint4		simpleapi_dollar_trestart;
GBLREF	uint4		dollar_trestart;
GBLREF	boolean_t	exit_handler_active;

/* Routine to drive ydb_tp_s() in a worker thread so YottaDB access is isolated. Note because this drives ydb_tp_s(),
 * we don't do any of the exclusive access checks here. The thread management itself takes care of most of that currently
 * but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_tp_s() still so no need for it here. The one
 * exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_tp_s() except for the addition of tptoken and errstr.
 */
int ydb_tp_st(uint64_t tptoken, ydb_buffer_t *errstr, ydb_tp2fnptr_t tpfn, void *tpfnparm, const char *transid,
		int namecount, const ydb_buffer_t *varnames)
{
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	boolean_t		nested_tp;
	int			retval;
	uint64_t		new_tptoken;
	libyottadb_routines	lydbrtn;
	int			rlbk_retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
	threaded_api_ydb_engine_lock(tptoken, errstr, LYDB_RTN_TP, &save_active_stapi_rtn, &save_errstr, &get_lock, &retval);
	if (YDB_OK != retval)
		return retval;
	for ( ; ;) /* for loop only there to let us break from various cases without having a deep if-then-else structure */
	{
		nested_tp = (boolean_t)dollar_tlevel;
		assert((YDB_NOTTP == tptoken) || (tptoken == USER_VISIBLE_TPTOKEN(dollar_tlevel, stmTPToken)));
		if (YDB_NOTTP == tptoken)
			stmTPToken++;
		/*
		 * For the LYDB_RTN_TP_START or LYDB_RTN_TP_START_TLVL0 case, we do not invoke the user-defined
		 *	callback function so do not need to pass tpfn or tpfnparm.
		 * For the LYDB_RTN_TP_RESTART/LYDB_RTN_TP_COMMIT/LYDB_RTN_TP_RESTART_TLVL0/LYDB_RTN_TP_COMMIT_TLVL0
		 *	cases, we do not need ANY of the above 5 parameters.
		 * Once the "op_tstart" is done above, we do not need "transid", "namecount" and "varnames"
		 *	parameters for the later calls.
		 * Also, after each "ydb_tp_s_common" call ensure a LIBYOTTADB_DONE was done
		 *	asserting that TREF(libyottadb_active_rtn) is LYDB_RTN_NONE.
		 */
		assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
		/* Start the TP transaction by asking "ydb_tp_s_common" to do the "op_tstart" */
		lydbrtn = (!nested_tp ? LYDB_RTN_TP_START_TLVL0 : LYDB_RTN_TP_START);
		retval = ydb_tp_s_common(lydbrtn, (ydb_basicfnptr_t)NULL, (void *)NULL, transid, namecount, varnames);
		assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
		assert(YDB_TP_RESTART != retval);
		if (YDB_OK != retval)
		{
			if (!nested_tp && dollar_tlevel)
			{	/* If outermost TP had an error, rollback any TP that might have been created
				 * (we do not expect any since dollar_tlevel++ is done only after all error code
				 * paths have been checked in "op_tstart") before returning error.
				 */
				assert(FALSE);
				rlbk_retval = ydb_tp_s_common(LYDB_RTN_TP_ROLLBACK_TLVL0, (ydb_basicfnptr_t)NULL, (void *)NULL,
							(const char *)NULL, (int)0, (ydb_buffer_t *)NULL);
				assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
				assert(YDB_TP_ROLLBACK == rlbk_retval);
				/* Note: "retval" records the "op_tstart" error code while
				 *       "rlbk_retval" holds the "op_trollback" error code (we do not expect any).
				 * Use the "op_tstart" error code (the first error) in the call block.
				 */
			}
			break;
		}
		/* Now that dollar_tlevel would have been bumped up by the above "ydb_tp_s_common" call, derive new tptoken */
		new_tptoken = USER_VISIBLE_TPTOKEN(dollar_tlevel, stmTPToken);
		for ( ; ; )
		{	/* Loop to handle TP restarts */
			if (!nested_tp)
			{	/* Maintain "simpleapi_dollar_trestart" just like "ydb_tp_s_common"
				 * does for the "LYDB_RTN_TP" case.
				 */
				simpleapi_dollar_trestart = dollar_trestart;
			}
			/* Invoke the user-defined TP callback function */
			retval = (*tpfn)(new_tptoken, errstr, tpfnparm);
			if (exit_handler_active)
			{	/* The most common way to get here is if a Go routine had a defer/recover() block to catch a fatal
				 * signal panic (asynchronous as Go defines them) in a TP callback routine and then returns to the
				 * engine from the TP callback routine. But similar things can be done in some other languages as
				 * well. Either way, the exit handler has run so we shouldn't be using this engine. If the exit
				 * handler was run due to a fatal signal coming in, then the engine is already unlocked so we can
				 * just return. But if the exit handler was run for some other reason, we probably still do hold
				 * the engine lock. At this point, only Go releases the lock but if that changes, then this code
				 * needs to change also.
				 */
				SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(YDB_ERR_CALLINAFTERXIT, errstr);
				if (YDB_MAIN_LANG_GO != ydb_main_lang)
					threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr,
								       get_lock);
				return YDB_ERR_CALLINAFTERXIT;
			}
			if (YDB_OK == retval)
			{	/* Commit the TP transaction by asking "ydb_tp_s_common" to do the "op_tcommit" */
				lydbrtn = (!nested_tp ? LYDB_RTN_TP_COMMIT_TLVL0 : LYDB_RTN_TP_COMMIT);
				retval = ydb_tp_s_common(lydbrtn, (ydb_basicfnptr_t)NULL, (void *)NULL,
							(const char *)NULL, (int)0, (ydb_buffer_t *)NULL);
			}
			assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
			if (nested_tp)
			{	/* If nested TP, return success/error code directly back to caller of "ydb_tp_st".
				 * But before that, do rollback if returning a non-zero error code.
				 * Do not do that in case of YDB_TP_RESTART error code as it is possible for
				 * a restart inside a nested TP inside ydb_ci_t() reset `dollar_tlevel` to 1 as part
				 * of `tp_restart(1,...)` call in `mdb_condition_handler.c`. Attempting an incremental
				 * rollback in that case would result in an error because dollar_tlevel is not > 1.
				 */
				if ((YDB_TP_RESTART != retval) && (YDB_OK != retval))
				{	/* Rollback the TP transaction by asking "ydb_tp_s_common" to do the "op_trollback" */
					assert(1 < dollar_tlevel);
					rlbk_retval = ydb_tp_s_common(LYDB_RTN_TP_ROLLBACK, (ydb_basicfnptr_t)NULL, (void *)NULL,
								(const char *)NULL, (int)0, (ydb_buffer_t *)NULL);
					assert((YDB_TP_ROLLBACK == rlbk_retval)
						|| (YDB_ERR_CALLINAFTERXIT == rlbk_retval)
							&& (retval == rlbk_retval) && dollar_tlevel);
				}
				assert(1 <= dollar_tlevel);
				break;
			}
			/* If we reach here, it means we are in the outermost TP */
			if (YDB_OK == retval)
			{	/* Outermost TP committed successfully. Return success to caller of "ydb_tp_st". */
				assert(0 == dollar_tlevel);
				break;
			}
			if (YDB_TP_RESTART != retval)
			{	/* Outermost TP and error code is not a TPRESTART.
				 * Return it directly to caller of "ydb_tp_st" but before that roll back the
				 *	TP transaction (if not already done).
				 * ROLLBACK the TP transaction by asking "ydb_tp_s_common" to do the "op_trollback".
				 * Note that it is possible "retval" is YDB_TP_ROLLBACK (e.g. if the callback
				 *	function returned YDB_TP_ROLLBACK).
				 */
				assert(1 >= dollar_tlevel);	/* See `dollar_tlevel` use below for why it can be 0 */
				if (dollar_tlevel)
				{	/* Rollback could have happened in some cases already (for example
					 * if "ydb_tp_s_common(LYDB_RTN_TP_COMMIT)" invocation failed with a GBLOFLOW
					 * error). We do not want to invoke a rollback again in that case since
					 * the tptoken no longer matches the current "dollar_tlevel" (would return with
					 * a YDB_ERR_INVTPTRANS error). Hence the "if (dollar_tlevel)" check above.
					 */
					rlbk_retval = ydb_tp_s_common(LYDB_RTN_TP_ROLLBACK_TLVL0, (ydb_basicfnptr_t)NULL,
								(void *)NULL, (const char *)NULL, (int)0, (ydb_buffer_t *)NULL);
					/* Note that it is possible the above returns a YDB_ERR_CALLINAFTERXIT error
					 * in case the YottaDB process has been asked to terminate.
					 * Take that into account in the below assert.
					 * In this case, the TP transaction would not be rolled back yet (so dollar_tlevel
					 * would still be non-zero) but it is okay to return in this case with this
					 * error code since the exiting logic will run the exit handler which will
					 * do the needed "op_trollback".
					 */
					assert((YDB_TP_ROLLBACK == rlbk_retval)
						|| (YDB_ERR_CALLINAFTERXIT == rlbk_retval)
							&& (retval == rlbk_retval) && dollar_tlevel);
				}
				assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
				/* Note: "retval" records the primary error code while
				 *       "rlbk_retval" holds the "op_trollback" error code (we do not expect any
				 *			except YDB_ERR_CALLINAFTERXIT in which case we expect retval
				 *			and rlbk_retval to be the same as asserted above).
				 * Use the "op_tstart" error code (the first error) in the call block.
				 */
				break;
			}
			/* Note: dollar_tlevel could be > 1 at this point but since retval is YDB_TP_RESTART, it is okay
			 * as the TP restart done below (using LYDB_RTN_TP_RESTART_TLVL0) will bring it back down to 1.
			 */
			/* Restart the outermost TP transaction by asking "ydb_tp_s_common" to do the "tp_restart" */
			retval = ydb_tp_s_common(LYDB_RTN_TP_RESTART_TLVL0, (ydb_basicfnptr_t)NULL,
						(void *)NULL, (const char *)NULL, (int)0, (ydb_buffer_t *)NULL);
			assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
			if (YDB_ERR_CALLINAFTERXIT == retval)
				break;	/* The YottaDB process has been asked to exit. Return right away. */
			assert(YDB_OK == retval);
		}
		break;
	}
	threaded_api_ydb_engine_unlock(tptoken, errstr, save_active_stapi_rtn, save_errstr, get_lock);
	assert((YDB_NOTTP != tptoken) || (YDB_TP_RESTART != retval));
	return retval;
}
