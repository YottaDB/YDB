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
#include "stringpool.h"		/* for "s2pool" prototype */
#include "op.h"			/* for "op_fnzwrite" prototype */

/* Routine to convert an input in the zwrite representation into string subscript representation.
 *
 * Parameters:
 *   zwr	- The input string in zwrite representation
 *   str	- The output string in subscript representation
 */
int ydb_zwr2str_s(ydb_buffer_t *zwr, ydb_buffer_t *str)
{
	mval		src, dst;
	boolean_t	error_encountered;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_ZWR2STR);	/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Do some validation */
	if (NULL == str)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
						LEN_AND_LIT("NULL str"), LEN_AND_LIT("ydb_zwr2str()"));
	src.mvtype = MV_STR;
	src.str.len = zwr->len_used;
	src.str.addr = zwr->buf_addr;
	op_fnzwrite(TRUE, &src, &dst);
	SET_YDB_BUFF_T_FROM_MVAL(str, &dst, "NULL str->buf_addr", "ydb_zwr2str_s()");
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	REVERT;
	return YDB_OK;
}
