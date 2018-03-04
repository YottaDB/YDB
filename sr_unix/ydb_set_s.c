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
#include "compiler.h"
#include "namelook.h"
#include "stringpool.h"
#include "libyottadb_int.h"
#include "outofband.h"

GBLREF	volatile int4	outofband;

/* Routine to set local, global and ISV values
 *
 * Parameters:
 *   varname	- Gives name of local, global or ISV variable
 *   subs_used	- Count of subscripts (if any else 0)
 *   subsarray  - an array of "subs_used" subscripts (not looked at if "subs_used" is 0)
 *   value	- Value to be set into local/global/ISV
 */
int ydb_set_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *value)
{
	boolean_t	error_encountered;
	gparam_list	plist;
	ht_ent_mname	*tabent;
	int		set_svn_index;
	lv_val		*lvvalp, *dst_lv;
	mname_entry	var_mname;
	mval		set_value, gvname, plist_mvals[YDB_MAX_SUBS + 1];
	ydb_buffer_t	null_ydb_buff;
	ydb_var_types	set_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_SET);	/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
		LIBYOTTADB_DONE;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	/* Do some validation */
	VALIDATE_VARNAME(varname, set_type, set_svn_index, TRUE);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if (NULL == value)
	{	/* Treat it as the null string */
		null_ydb_buff.len_used = 0;
		null_ydb_buff.buf_addr = NULL;
		value = &null_ydb_buff;
	} else
	{
		if (IS_INVALID_YDB_BUFF_T(value))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				      LEN_AND_LIT("Invalid value"), LEN_AND_LIT("ydb_set_s()"));
		CHECK_MAX_STR_LEN(value);		/* Generates error is value is too long */
	}
	/* Separate actions depending on the type of SET being done */
	switch(set_type)
	{
		case LYDB_VARREF_LOCAL:
			/* Set the given local variable with the given value.
			 *
			 * Note need to rebuffer EVERYTHING - the varname, subscripts and of course value are all potentially
			 * copied via saving name and address.
			 */
			FIND_BASE_VAR_UPD(varname, &var_mname, tabent, lvvalp);	/* Locate the base var lv_val in curr_symval */
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
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, TRUE, 1, "ydb_set_s()");
				dst_lv = (lv_val *)callg((callgfnptr)op_putindx, &plist);	/* Locate/create node */
			}
			SET_MVAL_FROM_YDB_BUFF_T(&dst_lv->v, value);	/* Set value into located/created node */
			s2pool(&(dst_lv->v.str));			/* Rebuffer in stringpool for protection */
			RECORD_MSTR_FOR_GC(&(dst_lv->v.str));
			break;
		case LYDB_VARREF_GLOBAL:
			/* Set the given global variable with the given value. We do this by:
			 *   1. Drive "op_gvname" with the global name and all subscripts to setup the key we need to access
			 *      the global node.
			 *   2. Drive "op_gvput" to place value into target global node
			 * Note no need to rebuffer the inputs here as they won't live in the stringpool once set.
			 */
			gvname.mvtype = MV_STR;
			gvname.str.addr = varname->buf_addr + 1;	/* Point past '^' to var name */
			gvname.str.len = varname->len_used - 1;
			plist.arg[0] = &gvname;
			/* Setup plist (which would point to plist_mvals[] array) for callg invocation of op_gvname */
			COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 1, "ydb_set_s()");
			callg((callgfnptr)op_gvname, &plist);		/* Drive "op_gvname" to create key */
			SET_MVAL_FROM_YDB_BUFF_T(&set_value, value);	/* Put value to set into mval for "op_gvput" */
			op_gvput(&set_value);				/* Save the global value */
			break;
		case LYDB_VARREF_ISV:
			/* Set the given ISV (subscripts not currently supported) with the given value.
			 * Note need to rebuffer the input value as the addr/length are directly copied in many cases.
			 */
			SET_MVAL_FROM_YDB_BUFF_T(&set_value, value);	/* Setup mval with target value */
			s2pool(&set_value.str);				/* Rebuffer in stringpool for protection */
			RECORD_MSTR_FOR_GC(&set_value.str);
			op_svput(set_svn_index, &set_value);
			break;
		default:
			assertpro(FALSE);
	}
	TREF(sapi_mstrs_for_gc_indx) = 0; /* mstrs in this array (added by RECORD_MSTR_FOR_GC) no longer need to be protected */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
