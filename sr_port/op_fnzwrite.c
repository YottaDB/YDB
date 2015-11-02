/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
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

/* Routine to return a string in zwrite format */
void op_fnzwrite(mval* src, mval* dst)
{
	int		dst_len, str_len;

	MV_FORCE_STR(src);
	MV_FORCE_NUM(src);
	if MV_IS_CANONICAL(src)
		*dst = *src;
	else
	{
		str_len = ZWR_EXP_RATIO(src->str.len);
		ENSURE_STP_FREE_SPACE((int)str_len);
		DBG_MARK_STRINGPOOL_UNEXPANDABLE;
		format2zwr((sm_uc_ptr_t)src->str.addr, src->str.len, (uchar_ptr_t)stringpool.free, &dst_len);
		DBG_MARK_STRINGPOOL_EXPANDABLE;
		if (MAX_STRLEN < dst_len)
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
		dst->str.addr = (char *)stringpool.free;	/* deferred in case dst == str */
		dst->str.len = dst_len;
		dst->mvtype = MV_STR;
		assert((unsigned char *)(dst->str.addr + dst_len) <= stringpool.top);
		stringpool.free = (unsigned char *)(dst->str.addr + dst_len);
	}
}
