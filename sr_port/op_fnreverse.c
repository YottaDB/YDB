/****************************************************************
 *      Copyright 2001, 2009 Fidelity Information Services, Inc        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;

/*
 * -----------------------------------------------
 * op_fnreverse()
 * MUMPS Reverse String function
 *
 * Arguments:
 *	src	- Pointer to Source string mval
 *	dst	- Pointer to destination mval to save the inverted string
 *
 * Return:
 *	none
 * -----------------------------------------------
 */
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
GBLREF	boolean_t	badchar_inhibit;

/* Unicode character-oriented $REVERSE (for $ZCHSET="UTF-8") */
void op_fnreverse(mval *src, mval *dst)
{
	unsigned char	*srcptr, *srctop, *dstptr, *dsttop;
	int		char_len, chlen;

	MV_FORCE_STR(src);
	ENSURE_STP_FREE_SPACE(src->str.len);
	dstptr = stringpool.free + src->str.len;
	srcptr = (unsigned char *)src->str.addr;
	srctop = (unsigned char *)src->str.addr + src->str.len;
	for (char_len = 0; srcptr < srctop; srcptr += chlen, ++char_len)
	{
		if (!UTF8_VALID(srcptr, srctop, chlen) && !badchar_inhibit)
			utf8_badchar(0, srcptr, srctop, 0, NULL);
		assert(chlen > 0 && chlen <= 4);
		switch (chlen) /* byte length of next character */
		{ /* NOTE: all fall-thru's below */
			case 4: *--dstptr = srcptr[3];
			case 3: *--dstptr = srcptr[2];
			case 2: *--dstptr = srcptr[1];
			case 1: *--dstptr = srcptr[0];
				break;
		}
	}
	assert(dstptr == stringpool.free);
	stringpool.free += src->str.len;
	MV_INIT_STRING(dst, src->str.len, dstptr);

	/* set character length of both source and destination mvals */
	dst->mvtype |= MV_UTF_LEN;
	dst->str.char_len = char_len;
	assert(!(MV_UTF_LEN & src->mvtype) || char_len == src->str.char_len);
	if (!(MV_UTF_LEN & src->mvtype))
	{
		src->mvtype |= MV_UTF_LEN;
		src->str.char_len = char_len;
	}
}
#endif /* UNICODE_SUPPORTED */

/* byte-oriented $REVERSE (for $ZCHSET="M") */
void op_fnzreverse(mval *src, mval *dst)
{
	int	lcnt;
	char    *in, *out;

	MV_FORCE_STR(src);
	ENSURE_STP_FREE_SPACE(src->str.len);
	out = (char *)stringpool.free;
	stringpool.free += src->str.len;
	in = src->str.addr + src->str.len * SIZEOF(char);
	dst->mvtype = MV_STR;
	dst->str.addr = out;
	dst->str.len = src->str.len;
	for (lcnt = src->str.len; lcnt > 0; lcnt--)
		*out++ = *--in;
	return;
}
