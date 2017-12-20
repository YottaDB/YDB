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
#include <stdarg.h>

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
#include "libydberrors.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

/* Routine to get local, global and ISV values
 *
 * Parameters:
 *   value	- Value fetched from local/global/ISV variable stored here (if room)
 *   subs_used	- Count of subscripts (if any else 0)
 *   varname	- Gives name of local, global or ISV variable
 *   subscrN	- a list of 0 or more ydb_buffer_t subscripts follows varname in the parm list
 *
 * Note unlike "ydb_set_s", none of the input subscript need rebuffering in this routine
 * as they are not ever being used to create a new node or are otherwise kept for any reason by the
 * YottaDB runtime routines. But the input varname does need rebuffering as it is possible the local
 * varname does not yet exist in the current symbol table (curr_symval) in which case we do want
 * that to store a name string pointing to the stringpool rather than user-pointed C program storage
 * which could change after the current ydb_*_s() call.
 */
int ydb_get_s(ydb_buffer_t *value, int subs_used, ydb_buffer_t *varname, ...)
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
	TREF(sapi_mstrs_for_gc_indx) = 0;			/* Clear any previously used entries */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Do some validation */
	VALIDATE_VARNAME(varname, get_type, get_svn_index, FALSE);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VARNAMEINVALID);
	if (YDB_MAX_SUBS < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if ((NULL == value) || (NULL == value->buf_addr) || (0 == value->len_alloc))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NORETBUFFER, RTS_ERROR_LITERAL("ydb_get_s()"));
	/* Separate actions depending on type of GET being done */
	switch(get_type)
	{
		case LYDB_VARREF_LOCAL:
			/* Get the given local variable value storing it in the provided buffer (if it fits) */
			FIND_BASE_VAR_NOUPD(varname, &var_mname, tabent, lvvalp);	/* Locate base var lv_val in curr_symval */
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
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, plist, plist_mvals, TRUE);
				src_lv = (lv_val *)callg((callgfnptr)op_getindx, &plist);	/* Locate node */
			}
			if (!LV_IS_VAL_DEFINED(src_lv))				/* Fetched value should be defined */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LVUNDEF);
			SET_BUFFER_FROM_LVVAL_VALUE(value, src_lv);		/* Copy value to return buffer */
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
			COPY_PARMS_TO_CALLG_BUFFER(subs_used, plist, plist_mvals, FALSE);
			callg((callgfnptr)op_gvname, &plist);		/* Drive "op_gvname" to create key */
			gotit = op_gvget(&get_value);			/* Fetch value into get_value - should signal UNDEF
									 * if value not found (and undef_inhibit not set)
									 */
			assert(gotit);
			SET_BUFFER_FROM_LVVAL_VALUE(value, &get_value);
			break;
		case LYDB_VARREF_ISV:
			/* Fetch the given ISV value (no subscripts supported) */
			op_svget(get_svn_index, &get_value);
			SET_BUFFER_FROM_LVVAL_VALUE(value, &get_value);
			break;
		default:
			assertpro(FALSE);
	}
	TREF(sapi_mstrs_for_gc_indx) = 0;		/* These mstrs are no longer protected */
	REVERT;
	return YDB_OK;
}
