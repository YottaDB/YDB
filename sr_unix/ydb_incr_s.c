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
#include "mvalconv.h"
#include "outofband.h"

GBLREF	volatile int4	outofband;

LITREF	mval		literal_one, literal_zero;

/* Routine to atomically increment the value of a given node/glvn with an input increment value
 *
 * Parameters:
 *   varname	- Gives name of local or global variable
 *   subs_used	- Count of subscripts (if any else 0)
 *   subsarray  - an array of "subs_used" subscripts (not looked at if "subs_used" is 0)
 *   increment  - increment value (converted from string to number if needed)
 *   ret_value	- Post-increment value of local/global variable stored/returned here (if room)
 */
int ydb_incr_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *increment, ydb_buffer_t *ret_value)
{
	boolean_t	error_encountered;
	gparam_list	plist;
	ht_ent_mname	*tabent;
	int		incr_svn_index;
	lv_val		*lvvalp, *dst_lv;
	mname_entry	var_mname;
	mval		gvname, plist_mvals[YDB_MAX_SUBS + 1];
	mval		*lv_mv, *increment_mv, increment_mval, *ret_mv, ret_mval;
	ydb_var_types	incr_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_INCR);			/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		LIBYOTTADB_DONE;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	/* Do some validation */
	VALIDATE_VARNAME(varname, incr_type, incr_svn_index, FALSE);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if (NULL == ret_value)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					LEN_AND_LIT("NULL ret_value"), LEN_AND_LIT("ydb_incr_s()"));
	if ((NULL == increment) || !increment->len_used)
		increment_mval = literal_one;
	else
	{
		if (IS_INVALID_YDB_BUFF_T(increment))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					LEN_AND_LIT("Invalid increment"), LEN_AND_LIT("ydb_incr_s()"));
		increment_mval.mvtype = MV_STR;
		increment_mval.str.addr = increment->buf_addr;
		increment_mval.str.len = increment->len_used;
	}
	increment_mv = &increment_mval;
	MV_FORCE_NUM(increment_mv);
	/* Separate actions depending on type of INCREMENT being done */
	switch(incr_type)
	{
		case LYDB_VARREF_LOCAL:
			/* Set the given local variable with the post-increment value. Create lvn if it does not already exist.
			 *
			 * Note need to rebuffer EVERYTHING - the varname, subscripts and value are all potentially
			 * copied via saving name and address.
			 */
			FIND_BASE_VAR_UPD(varname, &var_mname, tabent, lvvalp);	/* Locate base var lv_val in curr_symval */
			if (0 == subs_used)
				/* If no subscripts, this is where to store the value */
				dst_lv = lvvalp;
			else
			{	/* We have some subscripts - load the varname lv_val and the subscripts into our array for callg
				 * so we can drive "op_putindx" to locate the mval associated with those subscripts that need to
				 * be set.
				 */
				plist.arg[0] = lvvalp;				/* First arg is lv_val of the base var */
				/* Setup plist (which would point to plist_mvals[] array) for callg invocation of op_putindx */
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, TRUE, 1, "ydb_incr_s()");
				dst_lv = (lv_val *)callg((callgfnptr)op_putindx, &plist);	/* Locate/create node */
			}
			/* If we created this node just now, fill it with a 0 value */
			lv_mv = &dst_lv->v;
			if (!MV_DEFINED(lv_mv))
				*lv_mv = literal_zero;
			op_add(lv_mv, increment_mv, lv_mv);
			assert(!MV_IS_STRING(lv_mv));
			ret_mval = *lv_mv;
			ret_mv = &ret_mval;
			break;
		case LYDB_VARREF_GLOBAL:
			/* Find dstatus of the given global variable value. We do this by:
			 *   1. Drive "op_gvname" with the global name and all subscripts to setup the key we need to access
			 *      the global node.
			 *   2. Drive "op_gvdata" to fetch the information (status, descendants) we are interested in.
			 */
			gvname.mvtype = MV_STR;
			gvname.str.addr = varname->buf_addr + 1;	/* Point past '^' to var name */
			gvname.str.len = varname->len_used - 1;
			/* Setup plist (which would point to plist_mvals[] array) for callg invocation of op_gvname */
			if (0 < subs_used)
			{
				plist.arg[0] = &gvname;
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 1, "ydb_incr_s()");
				callg((callgfnptr)op_gvname, &plist);	/* Drive "op_gvname" to  key */
			} else
				op_gvname(1, &gvname);			/* Single parm call to get next global */
			ret_mv = &ret_mval;
			op_gvincr(increment_mv, ret_mv);
			break;
		case LYDB_VARREF_ISV:
			/* ISV references are not supported for this call */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
			break;
		default:
			assertpro(FALSE);
	}
	SET_YDB_BUFF_T_FROM_MVAL(ret_value, ret_mv, "NULL ret_value->buf_addr", "ydb_incr_s()"); /* Copy value to return buffer */
	TREF(sapi_mstrs_for_gc_indx) = 0; /* mstrs in this array (added by RECORD_MSTR_FOR_GC) no longer need to be protected */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
