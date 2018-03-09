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
#include "outofband.h"

GBLREF	volatile int4	outofband;

LITREF	mval	literal_zero;

/* Routine to locate the next subscript at a given level.
 *
 * Parameters:
 *   varname	- Gives name of local or global variable
 *   subs_used	- Count of subscripts (if any, else 0)
 *   subsarray  - an array of "subs_used" subscripts (not looked at if "subs_used" is 0)
 *   ret_value	- The "next" subscript is stored/returned here.
 *
 * Note unlike "ydb_set_s", none of the input subscript need rebuffering in this routine
 * as they are not ever being used to create a new node or are otherwise kept for any reason by the
 * YottaDB runtime routines.
 */
int ydb_subscript_next_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value)
{
	boolean_t	error_encountered;
	gparam_list	plist;
	ht_ent_mname	*tabent;
	int		get_svn_index;
	lv_val		*lvvalp, *ord_lv;
	mname_entry	var_mname;
	mval		*subval, nextsub, *nextsub_mv, varnamemv, gvname, plist_mvals[YDB_MAX_SUBS + 1];
	ydb_var_types	get_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_SUBSCRIPT_NEXT);	/* Note: macro could "return" from this function in case of errors */
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
					LEN_AND_LIT("NULL ret_value"), LEN_AND_LIT("ydb_subscript_next_s()"));
	/* Separate actions depending on type of variable for which the next subscript is being located */
	switch(get_type)
	{
		case LYDB_VARREF_LOCAL:
			/* Get the given local variable value storing it in the provided buffer (if it fits) */
			if (0 == subs_used)
			{	/* If no subscripts, use op_fnlvname() to locate the next local variable */
				varnamemv.mvtype = MV_STR;
				varnamemv.str.addr = varname->buf_addr;
				varnamemv.str.len = varname->len_used;
				op_fnlvname(&varnamemv, FALSE, &nextsub);
			} else
			{	/* We have some subscripts - subcases are as follows:
				 *   - If we have more than one subscript - load the varname lv_val and all the subscripts
				 *     except for the last one into our array for callg so we can drive "op_srchindx" to
				 *     locate the lv_val associated with those subscripts. The last key is passed with the
				 *     call to op_fnorder().
				 *   - If only one subscript, skip the call to op_srchindx() and just call op_fnorder() with
				 *     the single supplied subscript.
				 *
				 * First, locate the base lv_val.
				 */
				FIND_BASE_VAR_NOUPD(varname, &var_mname, tabent, lvvalp, ERR_LVUNDEF_OK_FALSE);
				if (NULL == lvvalp)
				{	/* Base local variable does not exist (ERR_LVUNDEF_OK_FALSE above is to ensure
					 * we do not issue a LVUNDEF error inside the FIND_BASE_VAR_NOUPD macro).
					 * Return 0 for "ydb_subscript_next_s" result.
					 */
					SET_YDB_BUFF_T_FROM_MVAL(ret_value, (mval *)&literal_zero,
								"NULL ret_value->buf_addr", "ydb_subscript_next_s()");
					break;
				}
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals,		\
									FALSE, 1, "ydb_subscript_next_s()");
				plist.n--;				/* Don't use last subscr in lookup */
				if (1 < subs_used)
				{	/* Drive op_srchindx() to find node at level prior to target level */
					plist.arg[0] = lvvalp;		/* First arg is lv_val of the base var */
					/* Setup plist (which would point to plist_mvals[] array) for callg invocation of
					 * op_srchindx().
					 */
					ord_lv = (lv_val *)callg((callgfnptr)op_srchindx, &plist);	/* Locate node */
				} else
					/* We have a single subscript so use base var as input lv_val to op_fnorder() */
					ord_lv = lvvalp;
				subval = (mval *)plist.arg[plist.n];	/* Should give us subscript we didn't use above */
				op_fnorder(ord_lv, subval, &nextsub);
				nextsub_mv = &nextsub;
				MV_FORCE_STR(nextsub_mv);
			}
			SET_YDB_BUFF_T_FROM_MVAL(ret_value, &nextsub, "NULL ret_value->buf_addr", "ydb_subscript_next_s()");
			break;
		case LYDB_VARREF_GLOBAL:
			/* Global variable subscript-next processing is the same regardless of argument count:
			 *   1. Drive "op_gvname" with the global name and all subscripts to setup the key we need to access
			 *      the global node.
			 *   2. Drive "op_gvorder" to fetch the next subscript at this level.
			 *
			 * Note no need to rebuffer any of the inputs here as they won't live in the stringpool once set.
			 */
			gvname.mvtype = MV_STR;
			gvname.str.addr = varname->buf_addr + 1;	/* Point past '^' to var name */
			gvname.str.len = varname->len_used - 1;
			/* Setup plist (which would point to plist_mvals[] array) for callg invocation of op_gvname */
			if (0 < subs_used)
			{
				plist.arg[0] = &gvname;
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals,		\
									FALSE, 1, "ydb_subscript_next_s()");
				callg((callgfnptr)op_gvname, &plist);	/* Drive "op_gvname" to create key */
			} else
				op_gvname(1, &gvname);			/* Single parm call to get next global */
			op_gvorder(&nextsub);				/* Locate next subscript this level */
			SET_YDB_BUFF_T_FROM_MVAL(ret_value, &nextsub, "NULL ret_value->buf_addr", "ydb_subscript_next_s()");
			break;
		case LYDB_VARREF_ISV:
			/* ISV references are not supported for this call */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
			break;
		default:
			assertpro(FALSE);
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
