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

#include "gtm_string.h"

#include "libyottadb_int.h"
#include "callg.h"
#include "op.h"
#include "error.h"
#include "stringpool.h"
#include "outofband.h"

GBLREF	volatile int4	outofband;

/* Routine to get local, global and ISV values
 *
 * Parameters:
 *   varname	- Gives name of local, global or ISV variable
 *   subs_used	- Count of subscripts (if any else 0)
 *   subsarray  - an array of "subs_used" subscripts (not looked at if "subs_used" is 0)
 *   ret_value	- Value fetched from local/global/ISV variable stored/returned here (if room)
 *
 * Note unlike "ydb_set_s", none of the input subscript need rebuffering in this routine
 * as they are not ever being used to create a new node or are otherwise kept for any reason by the
 * YottaDB runtime routines.
 */
int ydb_delete_excl_s(int namecount, ydb_buffer_t *varnames)
{
	boolean_t	error_encountered;
	gparam_list	plist;
	ydb_buffer_t	*curvarname;
	mval		plist_mvals[YDB_MAX_NAMES], *mvalp;
	void		**parmp, **parmp_top;
	char		buff[256];		/* sprintf() buffer */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_DELETE_EXCL);	/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have never become non-zero and even if it did,
								 * it should have been cleared by "ydb_simpleapi_ch".
								 */
		LIBYOTTADB_DONE;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	/* If the varname count is zero, this implies a local var kill-all. Check for that before attempting to
	 * validate a name that may not be specified.
	 */
	if (0 > namecount)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVNAMECOUNT, 2, RTS_ERROR_LITERAL("ydb_delete_excl_s()"));
	if (0 == namecount)
	{	/* Special case - no varname supplied so drive kill-all of local variables */
		op_killall();
		LIBYOTTADB_DONE;
		REVERT;
		return YDB_OK;
	}
	if (YDB_MAX_NAMES < namecount)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_NAMECOUNT2HI, 3,
						RTS_ERROR_LITERAL("ydb_delete_excl_s()"), YDB_MAX_NAMES);
	/* Run through the array creating mvals to hold the list of names to be excluded. Note, normally, we would use
	 * the COPY_PAMS_TO_CALLG_BUFFER() macro for this but in this case, we also need to validate each mval that it has
	 * a valid variable name in it so we do the parm copy "manually".
	 */
	for (curvarname = varnames, parmp = &plist.arg[0], parmp_top = parmp + namecount, mvalp = &plist_mvals[0];
	     parmp < parmp_top;
	     parmp++, mvalp++, curvarname++)

	{	/* Validate each name to make sure is well formed */
		if (IS_INVALID_YDB_BUFF_T(curvarname))
		{
			SPRINTF(buff, "Invalid varname array (index %d)", curvarname - varnames);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				      LEN_AND_STR(buff), LEN_AND_LIT("ydb_delete_excl_s()"));
		}
		if (YDB_MAX_IDENT < curvarname->len_used)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_VARNAME2LONG, 1, YDB_MAX_IDENT);
  		VALIDATE_MNAME_C1(curvarname->buf_addr, curvarname->len_used);	/* Validates varname and that it is a local var */
		SET_MVAL_FROM_YDB_BUFF_T(mvalp, curvarname);
		*parmp = mvalp;
	}
	plist.n = namecount;
	/* Drive the exclusive kill call */
	callg((callgfnptr)op_xkill, &plist);
	/* All done - return to caller */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
