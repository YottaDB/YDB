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

/* Routine to convert an input string into the zwrite representation.
 *
 * Parameters:
 *   str	- The input string in subscript representation
 *   zwr	- The output string in zwrite representation
 */
int ydb_str2zwr_s(ydb_buffer_t *str, ydb_buffer_t *zwr)
{
	mval		src, dst;
	boolean_t	error_encountered, save_gtm_utf8_mode;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 == str->len_used)
	{
		zwr->len_used = 0;
		return YDB_OK;
	}
	/* We want the zwrite representation to always have $C(x) where x < 256 and hence switch to M mode unconditionally */
	save_gtm_utf8_mode = gtm_utf8_mode;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_STR2ZWR);	/* Note: macro could "return" from this function in case of errors */
	TREF(sapi_mstrs_for_gc_indx) = 0;		/* No mstrs reserved yet */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
		gtm_utf8_mode = save_gtm_utf8_mode;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Do some validation */
	if ((NULL == str->buf_addr) || (0 == str->len_alloc))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NORETBUFFER, 2, RTS_ERROR_LITERAL("ydb_str2zwr()"));
		/* Separate actions depending on type of variable for which the next subscript is being located */
	src.mvtype = MV_STR;
	src.str.len = str->len_used;
	src.str.addr = str->buf_addr;
	s2pool(&src.str);		/* Rebuffer in stringpool for protection */
	RECORD_MSTR_FOR_GC(&src.str);
	op_fnzwrite(FALSE, &src, &dst);
	SET_BUFFER_FROM_LVVAL_VALUE(zwr, &dst);
	TREF(sapi_mstrs_for_gc_indx) = 0;		/* No need to protect "src.str" anymore */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
	gtm_utf8_mode = save_gtm_utf8_mode;
	REVERT;
	return YDB_OK;
}
