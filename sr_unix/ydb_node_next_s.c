/****************************************************************
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
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
#include "deferred_events_queue.h"

GBLREF	volatile int4	outofband;
GBLREF	boolean_t	yed_lydb_rtn;

/* Routine to locate the next node at any level (i.e. forward $query).
 *
 * Parameters:
 *   varname	    - Gives name of local or global variable
 *   subs_used	    - Count of subscripts (if any else 0) in input node
 *   subsarray	    - an array of "subs_used" subscripts in input node (not looked at if "subs_used" is 0)
 *   ret_subs_used  - Count of subscripts in returned/next node
 *   ret_subsarray  - an array of "*ret_subs_used" subscripts in returned/next node (not looked at if "*ret_subs_used" is 0)
 *
 * Note unlike "ydb_set_s", none of the input varname or subscripts need rebuffering in this routine
 * as they are not ever being used to create a new node or are otherwise kept for any reason by the
 * YottaDB runtime routines.
 */
int ydb_node_next_s(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	boolean_t	error_encountered;
	gparam_list	plist;
	ht_ent_mname	*tabent;
	int		nodenext_svn_index, status;
	lv_val		*lvvalp, *next_lv;
	mname_entry	var_mname;
	mval		varnamemv, gvname, plist_mvals[YDB_MAX_SUBS + 1];
	ydb_var_types	nodenext_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!yed_lydb_rtn)	/* yed_lydb_rtn is TRUE if called by ydb_encode_s() */
	{
		VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
						 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
						 * scenarios from not resetting this global variable even though this function
						 * returns.
						 */
		/* Verify entry conditions, make sure YDB CI environment is up etc. */
		LIBYOTTADB_INIT(LYDB_RTN_NODE_NEXT, (int));	/* Note: macro could "return" from this function in case
								 * of errors
								 */
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_query_node_subs_cnt));	/* should have been cleared by "ydb_simpleapi_ch" */
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	/* Do some validation */
	VALIDATE_VARNAME(varname, subs_used, FALSE, LYDB_RTN_NODE_NEXT, -1, nodenext_type, nodenext_svn_index);
	if (NULL == ret_subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			      LEN_AND_LIT("NULL ret_subs_used"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_NODE_NEXT)));
	status = YDB_OK;
	/* Separate actions depending on type of variable for which the next subscript is being located */
	switch(nodenext_type)
	{
		case LYDB_VARREF_LOCAL:
			/* Get the given local variable value storing it in the provided buffer (if it fits) */
			FIND_BASE_VAR_NOUPD(varname, &var_mname, tabent, lvvalp); /* Locate base lv_val */
			if (NULL == lvvalp)
			{	/* Base local variable does not exist. Return YDB_ERR_NODEEND for "ydb_node_next_s" result. */
				*ret_subs_used = 0;	/* No values returned */
				status = YDB_ERR_NODEEND;
				break;
			}
			varnamemv.mvtype = MV_STR;
			varnamemv.str.addr = varname->buf_addr;
			varnamemv.str.len = varname->len_used;
			if (0 == subs_used)
			{	/* If no subscripts, no need for callg() overhead */
				op_fnquery(3, NULL, &varnamemv, lvvalp);
			} else
			{	/* We have some subscripts - feed them all to op_fnquery */
				plist.arg[0] = NULL;			/* arg1: destination mval not supplied in simpleAPI mode */
				plist.arg[1] = &varnamemv;		/* arg2: varname mval */
				plist.arg[2] = lvvalp;			/* arg3: varname lv_val */
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 3,
							LYDBRTNNAME(LYDB_RTN_NODE_NEXT));
				callg((callgfnptr)op_fnquery, &plist);	/* Drive "op_fnquery" to locate next node */
			}
			status = sapi_return_subscr_nodes(ret_subs_used, ret_subsarray, (char *)LYDBRTNNAME(LYDB_RTN_NODE_NEXT));
			break;
		case LYDB_VARREF_GLOBAL:
			/* Global variable subscript-next processing is the same regardless of argument count:
			 *   1. Drive "op_gvname" with the global name and all subscripts to setup the key we need to access
			 *      the global node.
			 *   2. Drive "op_gvquery" to fetch the next node in the tree of the given global.
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
				COPY_PARMS_TO_CALLG_BUFFER(subs_used, subsarray, plist, plist_mvals, FALSE, 1,
							LYDBRTNNAME(LYDB_RTN_NODE_NEXT));
				callg((callgfnptr)op_gvname, &plist);	/* Drive "op_gvname" to create key */
			} else
				op_gvname(1, &gvname);			/* Single parm call to get next global */
			op_gvquery(NULL);				/* Locate next subscript this level */
			status = sapi_return_subscr_nodes(ret_subs_used, ret_subsarray, (char *)LYDBRTNNAME(LYDB_RTN_NODE_NEXT));
			break;
		case LYDB_VARREF_ISV:
			/* The VALIDATE_VARNAME macro call done above should have already issued an error in this case */
		default:
			assertpro(FALSE);
			break;
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	TREF(sapi_query_node_subs_cnt) = 0;
	if (!yed_lydb_rtn)	/* yed_lydb_rtn is TRUE if called by ydb_encode_s() */
		LIBYOTTADB_DONE;
	REVERT;
	return status;
}
