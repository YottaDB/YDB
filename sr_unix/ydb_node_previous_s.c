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

#include <stdarg.h>
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

/* Routine to locate the previous node at any level (i.e. reverse $query).
 *
 * Parameters:
 *   varname	    - Gives name of local or global variable
 *   subs_used	    - Count of subscripts (if any else 0) in input node
 *   subsarray	    - an array of "subs_used" subscripts in input node (not looked at if "subs_used" is 0)
 *   ret_subs_used  - Count of subscripts in returned/previous node
 *   ret_subsarray  - an array of "*ret_subs_used" subscripts in returned/previous node (not looked at if "*ret_subs_used" is 0)
 *
 * Note unlike "ydb_set_s", none of the input varname or subscripts need rebuffering in this routine
 * as they are not ever being used to create a new node or are otherwise kept for any reason by the
 * YottaDB runtime routines.
 */
int ydb_node_previous_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray,
			int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	boolean_t	error_encountered;
	gparam_list	plist;
	ht_ent_mname	*tabent;
	int		nodeprev_svn_index;
	lv_val		*lvvalp, *prev_lv;
	mname_entry	var_mname;
	mval		varnamemv, gvname, plist_mvals[YDB_MAX_SUBS + 1];
	ydb_var_types	nodeprev_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_NODE_PREVIOUS);	/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_query_node_subs_cnt));	/* Should have been cleared by "ydb_simpleapi_ch" */
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		LIBYOTTADB_DONE;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	/* Do some validation */
	VALIDATE_VARNAME(varname, nodeprev_type, nodeprev_svn_index, FALSE);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if (NULL == ret_subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			      LEN_AND_LIT("NULL ret_subs_used"), LEN_AND_LIT("ydb_node_previous_s()"));
	/* Separate actions depending on type of variable for which the previous subscript is being located */
	switch(nodeprev_type)
	{
		case LYDB_VARREF_LOCAL:
			/* Get the given local variable value storing it in the provided buffer (if it fits) */
			FIND_BASE_VAR_NOUPD(varname, &var_mname, tabent, lvvalp, ERR_LVUNDEF_OK_FALSE); /* Locate base lv_val */
			if (NULL == lvvalp)
			{	/* Base local variable does not exist (ERR_LVUNDEF_OK_FALSE above is to ensure
				 * we do not issue a LVUNDEF error inside the FIND_BASE_VAR_NOUPD macro).
				 * Return 0 for "ydb_node_previous_s" result.
				 */
				*ret_subs_used = YDB_NODE_END;
				break;
			}
			varnamemv.mvtype = MV_STR;
			varnamemv.str.addr = varname->buf_addr;
			varnamemv.str.len = varname->len_used;
			if (0 == subs_used)
			{	/* If no subscripts, use op_fnlvname() to locate the previous local variable */
				op_fnreversequery(3, NULL, &varnamemv, lvvalp);
			} else
			{	/* We have some subscripts - feed them all to op_fnquery */
				plist.arg[0] = NULL;			/* arg1: destination mval not supplied in simpleAPI mode */
				plist.arg[1] = &varnamemv;		/* arg2: varname mval */
				plist.arg[2] = lvvalp;			/* arg3: varname lv_val */
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 3, "ydb_node_previous_s()");
				callg((callgfnptr)op_fnreversequery, &plist);	/* Drive "op_fnreversequery" to locate previous node */
			}
			sapi_return_subscr_nodes(ret_subs_used, ret_subsarray, "ydb_node_previous_s()");
			break;
		case LYDB_VARREF_GLOBAL:
			/* Global variable subscript-previous processing is the same regardless of argument count:
			 *   1. Drive "op_gvname" with the global name and all subscripts to setup the key we need to access
			 *      the global node.
			 *   2. Drive "op_gvreversequery" to fetch the previous node in the tree of the given global.
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
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 1, "ydb_node_previous_s()");
				callg((callgfnptr)op_gvname, &plist);	/* Drive "op_gvname" to create key */
			} else
				op_gvname(1, &gvname);			/* Single parm call to locate specified global */
			op_gvreversequery(NULL);			/* Locate previous subscript this level */
			sapi_return_subscr_nodes(ret_subs_used, ret_subsarray, "ydb_node_previous_s()");
			break;
		case LYDB_VARREF_ISV:
			/* ISV references are not supported for this call */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
			break;
		default:
			assertpro(FALSE);
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	TREF(sapi_query_node_subs_cnt) = 0;
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
