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

#include "libyottadb_int.h"

/* Routine to take the results from the lower level routine doing the work for our simpleAPI call that are in the
 * global output array TREF(sapi_query_node_subs). The mstrs in this array are known to G/C. Copy the mstr data to
 * the user's supplied buffers stopping when/if we hit an error to return what we have.
 *
 * Parameters:
 *   ret_sub_cnt   - Address of:
 *       on input:    Count of total available output subscripts for output
 *       on output:   Count of subscripts returned to user
 *   ret_subsarray - Address of an array of ydb_buffer_t subscript descriptors
 *   ydb_caller_fn - Name of function that is invoking it. Used as a parameter to PARAMINVALID error if issued.
 */
void sapi_return_subscr_nodes(int *ret_subs_used, ydb_buffer_t *ret_subsarray, char *ydb_caller_fn)
{
	ydb_buffer_t	*outsubp, *outsubp_top;
	mstr		*mstrp, *mstrp_top;
	int		outsubs;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 == TREF(sapi_query_node_subs_cnt))
	{	/* No subscripts were returned - set to YDB_NODE_END */
		*ret_subs_used = YDB_NODE_END;
		return;
	}
	if (-1 == TREF(sapi_query_node_subs_cnt))
	{	/* No subscripts were returned but we are legitimately returning the basevar name in a rever $query()
		 * so no YDB_NODE_END return code.
		 */
		*ret_subs_used = 0;			/* Just return 0 subscripts */
	}
	if (NULL == ret_subsarray)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			      LEN_AND_LIT("NULL ret_subsarray"), LEN_AND_STR(ydb_caller_fn));
	if (*ret_subs_used < TREF(sapi_query_node_subs_cnt))
	{	/* We have run out of subscripts - set the output subscript count to what it should
		 * be and drive our error.
		 */
		*ret_subs_used = TREF(sapi_query_node_subs_cnt);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_INSUFFSUBS, 3,
			      LEN_AND_STR(ydb_caller_fn), TREF(sapi_query_node_subs_cnt));
	}
	/* Results from the lower level routine doing the work for our simpleAPI call should be in the global
	 * output array TREF(sapi_query_node_subs). The mstrs here are know to G/C. Copy the mstr data to the
	 * user's supplied buffers stopping when/if we hit an error to return what we have.
	 */
	outsubs = *ret_subs_used;
	*ret_subs_used = 0;
	for (mstrp = TREF(sapi_query_node_subs), mstrp_top = mstrp + TREF(sapi_query_node_subs_cnt),
			outsubp = ret_subsarray, outsubp_top = outsubp + outsubs;
		mstrp < mstrp_top;
			mstrp++, outsubp++)
	{
		/* We have an output subscript, now see if its buffer is big enough */
		outsubp->len_used = mstrp->len;	/* How big it needs to be (set no matter what) */
		if (mstrp->len > outsubp->len_alloc)
			/* Buffer is too small - report an error */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVSTRLEN);
		memcpy(outsubp->buf_addr, mstrp->addr, mstrp->len);
		/* Now that everything is in place, bump the count of subscripts. This is left till now because
		 * when an error occurs this value is used as an INDEX to the broken subscript so we can only
		 * increment it after the rebuffering is complete.
		 */
		(*ret_subs_used)++;
	}
}
