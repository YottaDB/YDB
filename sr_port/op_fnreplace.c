/****************************************************************
 *								*
 * Copyright (c) 2024 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_string.h"
#include "op.h"
#include "stringpool.h"
#include "gtm_icu_api.h"
#include "gtm_conv.h"
#include "have_crit.h"
#include "ebc_xlat.h"

GBLREF spdesc stringpool;

error_def(ERR_MAXSTRLEN);

#ifdef UTF8_SUPPORTED
#include "gtm_utf8.h"

GBLREF	boolean_t	badchar_inhibit, gtm_utf8_mode;

void	op_fnreplace(mval *src, mval* substr, mval* rplc, mval* dst)
{
	boolean_t		utf8_match;
	char			*srcptr, *srctmp, *srctop, *subptr, *subtop;
	uint4			srccode, subcode, dstlen;
	unsigned char		*str_out;

	assert(gtm_utf8_mode);
	MV_FORCE_STR(src);
	MV_FORCE_STR(substr);
	MV_FORCE_STR(rplc);
	if ((0 == substr->str.len) || (0 == src->str.len) ||
			((substr->str.len == rplc->str.len) && (!memcmp(substr->str.addr, rplc->str.addr, rplc->str.len))))
	{
		assert(MAX_STRLEN >= src->str.len);
		dstlen = no_conversion(&src->str);
		MV_INIT_STRING(dst, dstlen, stringpool.free);
		stringpool.free += dst->str.len;
		return;
	}
	if (!badchar_inhibit)
	{       /* needed only to validate for BADCHARs */
		MV_FORCE_LEN(src);
		MV_FORCE_LEN(substr);
		MV_FORCE_LEN(rplc);
	} else
	{       /* but need some at least sorta valid length */
		MV_FORCE_LEN_SILENT(src);
		MV_FORCE_LEN_SILENT(substr);
		MV_FORCE_LEN_SILENT(rplc);
	}
	assert((0 <= src->str.char_len) && (MAX_STRLEN >= src->str.char_len));
	assert((0 <= substr->str.char_len) && (MAX_STRLEN >= substr->str.char_len));
	assert((0 <= rplc->str.char_len) && (MAX_STRLEN >= rplc->str.char_len));
	if ((src->str.len == src->str.char_len) && (substr->str.len == substr->str.char_len) &&
			(rplc->str.len == rplc->str.char_len))
	{
		op_fnzreplace_common(src, substr, rplc, dst);
		return;
	}
	ENSURE_STP_FREE_SPACE((DIVIDE_ROUND_UP(src->str.len, substr->str.len)) * (rplc->str.len + 1));
	str_out = (unsigned char *)stringpool.free;
	for (dstlen = 0, srcptr = src->str.addr, srctop = src->str.addr + src->str.len; srcptr < srctop; )
	{
		if ((srctop - srcptr) < substr->str.len)
		{
			dstlen += (srctop - srcptr);
			memcpy(str_out, srcptr, (srctop - srcptr));
			break;
		}
		utf8_match = TRUE;
		srctmp = srcptr;
		for (subptr = substr->str.addr, subtop = substr->str.addr + substr->str.len; subptr < subtop; )
		{
			srcptr = (char *)UTF8_MBTOWC(srcptr, srctop, srccode);
			subptr = (char *)UTF8_MBTOWC(subptr, subtop, subcode);
			if (srccode != subcode)
			{
				utf8_match = FALSE;
				/* Copy original chars to the stringpool */
				dstlen += (srcptr - srctmp);
				memcpy(str_out, srctmp, (srcptr - srctmp));
				str_out += (srcptr - srctmp);
				break;
			}
		}
		if (utf8_match)
		{	/* Copy rplc to the stringpool */
			dstlen += rplc->str.len;
			memcpy(str_out, rplc->str.addr, rplc->str.len);
			str_out += rplc->str.len;
		}
	}
	if (MAX_STRLEN < dstlen)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
	MV_INIT_STRING(dst, dstlen, stringpool.free);
	stringpool.free += dst->str.len;
}
#endif /* UTF8_SUPPORTED */

void	op_fnzreplace(mval *src, mval* substr, mval* rplc, mval* dst)
{
	int		dstlen;

	MV_FORCE_STR(src);
	MV_FORCE_STR(substr);
	MV_FORCE_STR(rplc);
	if ((0 == substr->str.len) || (0 == src->str.len) ||
			((substr->str.len == rplc->str.len) && (!memcmp(substr->str.addr, rplc->str.addr, rplc->str.len))))
	{
		assert(MAX_STRLEN >= src->str.len);
		dstlen = no_conversion(&src->str);
		MV_INIT_STRING(dst, dstlen, stringpool.free);
		stringpool.free += dst->str.len;
		return;
	}
	op_fnzreplace_common(src, substr, rplc, dst);
}

void	op_fnzreplace_common(mval *src, mval* substr, mval* rplc, mval* dst)
{
	int		dstlen, i;
	unsigned char	*str_out;

	ENSURE_STP_FREE_SPACE((DIVIDE_ROUND_UP(src->str.len, substr->str.len)) * (rplc->str.len + 1));
	for (i = 0, dstlen = 0, str_out = (unsigned char *)stringpool.free; i < src->str.len; )
	{
		if ((src->str.len - i) < substr->str.len)
		{
			dstlen += (src->str.len - i);
			if (MAX_STRLEN < dstlen)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
			memcpy(str_out, (src->str.addr + i), (src->str.len - i));
			break;
		}
		if (memcmp((src->str.addr + i), substr->str.addr, substr->str.len))
		{
			dstlen++;
			if (MAX_STRLEN < dstlen)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
			*str_out++ = src->str.addr[i++];
		} else
		{
			dstlen += rplc->str.len;
			if (MAX_STRLEN < dstlen)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
			memcpy(str_out, rplc->str.addr, rplc->str.len);
			str_out += rplc->str.len;
			i += substr->str.len;
		}
	}
	MV_INIT_STRING(dst, dstlen, stringpool.free);
	stringpool.free += dst->str.len;
}
