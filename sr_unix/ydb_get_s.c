/****************************************************************
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

#undef DEBUG_LIBYOTTADB		/* Change to #define to enable debugging - must be set prior to include of libyottadb_int.h */

#include "lv_val.h"
#include "hashtab_mname.h"
#include "callg.h"
#include "op.h"
#include "error.h"
#include "nametabtyp.h"
#include "namelook.h"
#include "stringpool.h"
#include "libyottadb_int.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
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
int ydb_get_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value)
{
	boolean_t	error_encountered;
	boolean_t	gotit;
	gparam_list	plist;
	ht_ent_mname	*tabent;
	int		get_svn_index;
	lv_val		*lvvalp, *src_lv;
	mname_entry	var_mname;
	mval		get_value, gvname, plist_mvals[YDB_MAX_SUBS + 1];
	ydb_var_types	get_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_GET);	/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have never become non-zero and even if it did,
								 * it should have been cleared by "ydb_simpleapi_ch".
								 */
		LIBYOTTADB_DONE;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	/* Do some validation */
	VALIDATE_VARNAME(varname, get_type, get_svn_index, FALSE);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if (NULL == ret_value)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
							LEN_AND_LIT("NULL ret_value"), LEN_AND_LIT("ydb_get_s()"));
	/* Separate actions depending on type of GET being done */
	switch(get_type)
	{
		case LYDB_VARREF_LOCAL:
			/* Get the given local variable value storing it in the provided buffer (if it fits) */
			FIND_BASE_VAR_NOUPD(varname, &var_mname, tabent, lvvalp, ERR_LVUNDEF_OK_TRUE);
					/* Locate base var lv_val in curr_symval. Issue LVUNDEF error if base lv does not exist. */
			if (0 == subs_used)
				/* If no subscripts, this is where to fetch the value from (if it exists) */
				src_lv = lvvalp;
			else
			{	/* We have some subscripts - load the varname lv_val and the subscripts into our array for callg
				 * so we can drive "op_putindx" to locate the mval associated with those subscripts that need to
				 * be set. Note "op_getindx" raises ERR_LVUNDEF if node is not found. Note that even if a node
				 * is found, it may not have a value associated with it which we need to detect and raise
				 * an LVUNDEF error for.
				 */
				plist.arg[0] = lvvalp;				/* First arg is lv_val of the base var */
				/* Setup plist (which would point to plist_mvals[] array) for callg invocation of op_getindx */
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 1, "ydb_get_s()");
				src_lv = (lv_val *)callg((callgfnptr)op_getindx, &plist);	/* Locate node */
			}
			if (!LV_IS_VAL_DEFINED(src_lv))				/* Fetched value should be defined */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LVUNDEF);
			/* Copy value to return buffer */
			SET_YDB_BUFF_T_FROM_MVAL(ret_value, &src_lv->v, "NULL ret_value->buf_addr", "ydb_get_s()");
			break;
		case LYDB_VARREF_GLOBAL:
			/* Fetch the given global variable value. We do this by:
			 *   1. Drive "op_gvname" with the global name and all subscripts to setup the key we need to access
			 *      the global node.
			 *   2. Drive "op_gvget" to fetch the value if the root exists (else drive an undef error).
			 *
			 * Note no need to rebuffer any of the inputs here as they won't live in the stringpool once set.
			 */
			gvname.mvtype = MV_STR;
			gvname.str.addr = varname->buf_addr + 1;	/* Point past '^' to var name */
			gvname.str.len = varname->len_used - 1;
			plist.arg[0] = &gvname;
			/* Setup plist (which would point to plist_mvals[] array) for callg invocation of op_gvname */
			COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 1, "ydb_get_s()");
			callg((callgfnptr)op_gvname, &plist);		/* Drive "op_gvname" to create key */
			gotit = op_gvget(&get_value);			/* Fetch value into get_value - should signal UNDEF
									 * if value not found (and undef_inhibit not set)
									 */
			assert(gotit);
			/* Copy value to return buffer */
			SET_YDB_BUFF_T_FROM_MVAL(ret_value, &get_value, "NULL ret_value->buf_addr", "ydb_get_s()");
			break;
		case LYDB_VARREF_ISV:
			/* Fetch the given ISV value (no subscripts supported) */
			op_svget(get_svn_index, &get_value);
			/* Copy value to return buffer */
			SET_YDB_BUFF_T_FROM_MVAL(ret_value, &get_value, "NULL ret_value->buf_addr", "ydb_get_s()");
			break;
		default:
			assertpro(FALSE);
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
