/****************************************************************
 *								*
 *	Copyright 2006, 2009 Fidelity Information Services, Inc	*
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
#include "min_max.h"
#include "op.h"
#include "gtm_utf8.h"

GBLREF spdesc		stringpool;
GBLREF	boolean_t	badchar_inhibit;

/*
 * ----------------------------------------------------------
 * Set version of $extract
 *
 * Arguments:
 *	src	- source mval
 *	expr	- expression string mval to be inserted into source
 *	schar	- starting character index to be replaced
 *	echar	- ending character index to be replaced
 *	dst	- destination mval where the result is saved.
 *
 * Return:
 *	none
 * ----------------------------------------------------------
 */
void op_setextract(mval *src, mval *expr, int schar, int echar, mval *dst)
{
	int		srclen, padlen, pfxlen, sfxoff, sfxlen, dstlen, bytelen, skip, char_len;
	unsigned char	*srcptr, *srcbase, *srctop, *straddr;

	error_def(ERR_MAXSTRLEN);

	padlen = pfxlen = sfxlen = 0;
	MV_FORCE_STR(expr);	/* Expression to put into piece place */
	if (MV_DEFINED(src))
	{
		MV_FORCE_STR(src);	/* Make sure is string prior to length check */
		srclen = src->str.len;
	} else	/* Source is not defined -- treat as a null string */
		srclen = 0;
	schar = MAX(schar, 1);	/* schar starts at 1 at a minimum */

	/* There are four cases in the spec:
	   1) schar > echar or echar < 1 -- glvn and naked indicator are not changed. This is
	                                    handled by generated code in m_set
	   2) echar >= schar-1 > $L(src) -- dst = src_$J("",schar-1-$L(src))_expr
	   3) schar-1 <= $L(src) < echar -- dst = $E(src,1,schar-1)_expr
	   4) All others                 -- dst = $E(src,1,schar-1)_expr_$E(src,echar+1,$L(src))
	*/
	srcbase = (unsigned char *)src->str.addr;
	srctop = srcbase + srclen;
	for (srcptr = srcbase, skip = schar - 1; (skip > 0 && srcptr < srctop); --skip)
	{ /* skip the first schar - 1 characters */
		if (!UTF8_VALID(srcptr, srctop, bytelen) && !badchar_inhibit)
			utf8_badchar(0, srcptr, srctop, 0, NULL);
		srcptr += bytelen;
	}
	pfxlen = (int)(srcptr - srcbase);
	if (skip > 0)
	{ /* Case #2: schar is past the string length. echar test handled in generated code.
	     Should be padded with as many spaces as characters remained to be skipped */
		padlen = skip;
	}
	for (skip = echar - schar + 1; (skip > 0 && srcptr < srctop); --skip)
	{ /* skip up to the character position echar */
		if (!UTF8_VALID(srcptr, srctop, bytelen) && !badchar_inhibit)
			utf8_badchar(0, srcptr, srctop, 0, NULL);
		srcptr += bytelen;
	}
	char_len = 0;
	if (skip <= 0)
	{ /* Case #4: echar is within the string length, suffix to be added */
		sfxoff = (int)(srcptr - srcbase);
		sfxlen = (int)(srctop - srcptr);
		if (!badchar_inhibit && sfxlen > 0)
		{ /* validate the suffix, and we can compute char_len as well */
			for (; (srcptr < srctop); ++char_len)
			{
				if (!UTF8_VALID(srcptr, srctop, bytelen))
					utf8_badchar(0, srcptr, srctop, 0, NULL);
				srcptr += bytelen;
			}
			MV_FORCE_LEN(expr);
			char_len += schar - 1 + expr->str.char_len;
		}
	}

	/* Calculate total string len */
	dstlen = pfxlen + padlen + expr->str.len + sfxlen;
	if (dstlen > MAX_STRLEN)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	ENSURE_STP_FREE_SPACE(dstlen);

	srcbase = (unsigned char *)src->str.addr;
	straddr = stringpool.free;

	if (0 < pfxlen)
	{ /* copy prefix */
		memcpy(straddr, srcbase, pfxlen);
		straddr += pfxlen;
	}
	if (0 < padlen)
	{ /* insert padding */
		memset(straddr, ' ', padlen);
		straddr += padlen;
	}
	if (0 < expr->str.len)
	{ /* copy expression */
		memcpy(straddr, expr->str.addr, expr->str.len);
		straddr += expr->str.len;
	}
	if (0 < sfxlen)
	{ /* copy suffix */
		memcpy(straddr, srcbase + sfxoff, sfxlen);
		straddr += sfxlen;
	}
	assert(straddr - stringpool.free == dstlen);
	MV_INIT_STRING(dst, straddr - stringpool.free, (char *)stringpool.free);
	if (0 < char_len)
	{
		dst->mvtype |= MV_UTF_LEN;
		dst->str.char_len = char_len;
	}
	stringpool.free = straddr;
}
