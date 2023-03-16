/****************************************************************
 *								*
 * Copyright (c) 2012-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2023 YottaDB LLC and/or its subsidiaries.	*
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
#include "stringpool.h"
#include "op.h"
#include "zshow.h"

error_def(ERR_MAXSTRLEN);

LITREF mval	literal_sqlnull;

/* Routine to return a string in zwrite format */
void op_fnzwrite(boolean_t direction, mval* src, mval* dst)
{
	boolean_t	ok, done;
	int		dst_len, str_len;
	mstr		tmp_mstr;

	MV_FORCE_STR(src);
	if (MV_IS_CANONICAL(src))
		*dst = *src;
	else
	{
		done = FALSE;
		if (direction)
		{	/* Check if it is the special string $ZYSQLNULL. If so transform it back to the mval `literal_sqlnull` */
			if ((DOLLAR_ZYSQLNULL_STRLEN == src->str.len)
				&& !memcmp(src->str.addr, DOLLAR_ZYSQLNULL_STRING, DOLLAR_ZYSQLNULL_STRLEN))
			{
				*dst = literal_sqlnull;
				done = TRUE;
			}
		} else if (MV_IS_SQLNULL(src))
		{	/* Source mval is $ZYSQLNULL. Convert it to the string literal $ZYSQLNULL and return. */
			ENSURE_STP_FREE_SPACE(DOLLAR_ZYSQLNULL_STRLEN);
			MEMCPY_LIT(stringpool.free, DOLLAR_ZYSQLNULL_STRING);
			dst->str.addr = (char *)stringpool.free;	/* deferred in case dst == str */
			dst->str.len = DOLLAR_ZYSQLNULL_STRLEN;
			dst->mvtype = MV_STR;
			stringpool.free += DOLLAR_ZYSQLNULL_STRLEN;
			done = TRUE;
		}
		if (!done)
		{
			str_len = direction ? src->str.len : ZWR_EXP_RATIO(src->str.len);
			ENSURE_STP_FREE_SPACE((int)str_len);
			DBG_MARK_STRINGPOOL_UNEXPANDABLE;
			if (direction)
			{
				tmp_mstr.addr = (char *)stringpool.free;
				ok = zwr2format(&src->str, &tmp_mstr);
				dst_len = ok ? tmp_mstr.len : 0;
				dst->str.addr = tmp_mstr.addr;
				DBG_MARK_STRINGPOOL_EXPANDABLE;
			} else
			{
				dst_len = str_len;
				format2zwr((sm_uc_ptr_t)src->str.addr, src->str.len, (uchar_ptr_t)stringpool.free, &dst_len);
				DBG_MARK_STRINGPOOL_EXPANDABLE;
				if (MAX_STRLEN < dst_len)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
				dst->str.addr = (char *)stringpool.free;	/* deferred in case dst == str */
			}
			dst->str.len = dst_len;
			dst->mvtype = MV_STR;
			assert((unsigned char *)(dst->str.addr + dst_len) <= stringpool.top);
			stringpool.free = (unsigned char *)(dst->str.addr + dst_len);
		}
	}
}
