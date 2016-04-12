/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

GBLREF spdesc stringpool;
error_def(ERR_MAXSTRLEN);

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
GBLREF	boolean_t	badchar_inhibit;

void op_fnj2(mval *src, int len, mval *dst)
{
	unsigned char 	*cp;
	int 		n, size;

	if (len > MAX_STRLEN)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);

	MV_FORCE_STR(src);
	MV_FORCE_LEN(src);
	n = len - src->str.char_len;
	if (n <= 0)
	{
		*dst = *src;
		dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	} else
	{
		size = src->str.len + n;
		if (size > MAX_STRLEN)
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
		ENSURE_STP_FREE_SPACE(size);
		cp = stringpool.free;
		stringpool.free += size;
		memset(cp, SP, n);
		memcpy(cp + n, src->str.addr, src->str.len);
		MV_INIT_STRING(dst, size, (char *)cp);
	}
	return;
}
#endif /* UNICODE_SUPPORTED */

void op_fnzj2(mval *src, int len, mval *dst)
{
	unsigned char	*cp;
	int 		n;

	if (len > MAX_STRLEN)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);

	MV_FORCE_STR(src);
	n = len - src->str.len;
	if (n <= 0)
	{
		*dst = *src;
		dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	} else
	{
		ENSURE_STP_FREE_SPACE(len);
		cp = stringpool.free;
		stringpool.free += len;
		memset(cp, SP, n);
		memcpy(cp + n, src->str.addr, src->str.len);
		MV_INIT_STRING(dst, len, (char *)cp);
	}
	return;
}
