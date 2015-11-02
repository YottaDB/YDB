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

GBLREF spdesc		stringpool;

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

void op_setzextract(mval *src, mval *expr, int schar, int echar, mval *dst)
{
	int		srclen, padlen, pfxlen, sfxoff, sfxlen, strlen;
	unsigned char	*srcptr, *pfx, *straddr;

	error_def(ERR_MAXSTRLEN);

	padlen = pfxlen = sfxlen = 0;
	MV_FORCE_STR(expr);	/* Expression to put into piece place */
	if (MV_DEFINED(src))
	{
		MV_FORCE_STR(src);	/* Make sure is string prior to length check */
		srclen = src->str.len;
	} else
	{	/* Source is not defined -- treat as a null string */
		srclen = 0;
	}
	schar = MAX(schar, 1);	/* schar starts at 1 at a minimum */

	/* There are four cases in the spec:

	   1) schar > echar or echar < 1 -- glvn and naked indicator are not changed. This is
	                                    handled by generated code in m_set
	   2) echar '< schar-1 > srclen  -- dst = src_$J("",schar-1-srclen)_expr
	   3) schar-1 '> srclen < echar  -- dst = $E(src,1,schar-1)_expr
	   4) All others                 -- dst = $E(src,1,schar-1)_expr_$E(src,echar+1,srclen)
	*/
	if ((schar - 1) > srclen)
	{	/* Case #2 -- echar test handled in generated code */
		pfxlen = srclen;
		padlen = schar - 1 - srclen;
		/* Note, no suffix, just expr after the padlen */
	} else
	{	/* (schar-1) <= srclen) (Case #3 and common part of Default case) */
		pfxlen = schar - 1;
		/* Test if truly default case */
		if (srclen >= echar)		/* If (srclen < echar) then was case 3 and we are done. else.. */
		{	/* Default case */
			sfxoff = echar;
			sfxlen = srclen - echar;
		}
	}

	/* Calculate total string len. delim_cnt has needed padding delimiters for null fields */
	strlen = pfxlen + padlen + expr->str.len + sfxlen;
	if (strlen > MAX_STRLEN)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	ENSURE_STP_FREE_SPACE(strlen);

	pfx = (unsigned char *)src->str.addr;
	straddr = stringpool.free;

	/* copy prefix */
	if (0 < pfxlen)
	{
		memcpy(straddr, pfx, pfxlen);
		straddr += pfxlen;
	}

	/* insert padding */
	while (padlen-- > 0)
		*straddr++ = ' ';

	/* copy expression */
	if (0 < expr->str.len)
	{
		memcpy(straddr, expr->str.addr, expr->str.len);
		straddr += expr->str.len;
	}

	/* copy suffix */
	if (0 < sfxlen)
	{
		memcpy(straddr, pfx + sfxoff, sfxlen);
		straddr += sfxlen;
	}

	assert(straddr - stringpool.free == strlen);
	dst->mvtype = MV_STR;
	dst->str.len = INTCAST(straddr - stringpool.free);
	dst->str.addr = (char *)stringpool.free;
	stringpool.free = straddr;
}
