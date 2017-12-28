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

#include "libyottadb_int.h"
#include "libydberrors.h"
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
	TREF(sapi_mstrs_for_gc_indx) = 0;		/* No mstrs reserved yet */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Do some validation */
	if ((NULL == str) || (NULL == str->buf_addr) || (0 == str->len_alloc))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NORETBUFFER, 2, RTS_ERROR_LITERAL("ydb_zwr2str()"));
	src.mvtype = MV_STR;
	src.str.len = zwr->len_used;
	src.str.addr = zwr->buf_addr;
	s2pool(&src.str);		/* Rebuffer in stringpool for protection */
	RECORD_MSTR_FOR_GC(&src.str);
	op_fnzwrite(TRUE, &src, &dst);
	SET_YDB_BUFF_T_FROM_MVAL(str, &dst);
	TREF(sapi_mstrs_for_gc_indx) = 0;		/* No need to protect "src.str" anymore */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
	REVERT;
	return YDB_OK;
}
