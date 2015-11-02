/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	badchar_inhibit;

void op_fnzsubstr(mval* src, int first, int byte_width, mval* dest)
{
	char		*srcbase, *srctop, *srcptr, *tmpptr;
	int		bytelen, skip;
	boolean_t	src_is_singlebyte;

	MV_FORCE_STR(src);
	MV_INIT(dest);
	dest->mvtype = MV_STR;
	if (first <= 0)
		first = 1;
	if (first > src->str.len || byte_width <= 0)
	{
		dest->str.len = 0;
		return;
	}
	srctop = src->str.addr + src->str.len;
	UNICODE_ONLY(if (!gtm_utf8_mode || (src_is_singlebyte = MV_IS_SINGLEBYTE(src)))) /* entirely single byte string */
		srcbase =  src->str.addr + first - 1;
#	ifdef UNICODE_SUPPORTED
	else
	{ /* generic extraction of a multi-byte string */
		for (srcbase = src->str.addr, skip = first - 1; (skip > 0 && srcbase < srctop); --skip)
		{ /* advance to the beginning of the character position 'first' */
			if (!UTF8_VALID(srcbase, srctop, bytelen) && !badchar_inhibit)
				UTF8_BADCHAR(0, srcbase, srctop, 0, NULL);
			srcbase += bytelen;
		}
		if (skip > 0)
		{ /* the first character position is past the last character */
			dest->str.len = 0;
			return;
		}
	}
#	endif
	dest->str.addr = srcbase;
	if (srctop - srcbase > byte_width)
	{
		srcptr = srcbase + byte_width;
#		ifdef UNICODE_SUPPORTED
		if (gtm_utf8_mode)
		{
			if (src_is_singlebyte)
			{ /* if source is entirely single byte, so is destination */
				dest->str.char_len = byte_width;
				dest->mvtype |= MV_UTF_LEN;
			} else
			{ /* adjust to the leading byte of the last character that contains the byte at srcptr */
				UTF8_LEADING_BYTE(srcptr, srcbase, tmpptr);
				srcptr = tmpptr;
			}
		}
#		endif
		dest->str.len = INTCAST(srcptr - srcbase);
	} else /* width exceeds the length, so return the rest of the entire string */
		dest->str.len = INTCAST(srctop - srcbase);
#	ifdef UNICODE_SUPPORTED
	if (gtm_utf8_mode && !src_is_singlebyte && !badchar_inhibit)
		MV_FORCE_LEN(dest); /* catch BADCHAR (if any) */
#	endif
}
