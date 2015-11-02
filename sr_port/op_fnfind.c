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
#include "matchc.h"
#include "mvalconv.h"
#include "op.h"

/*
 * -----------------------------------------------
 * op_fnfind()
 *
 * MUMPS Find function
 *
 * Arguments:
 *	src	- Pointer to Source string mval
 *	del	- Pointer to delimiter mval
 *	first	- starting index
 *	dst	- destination buffer to save the piece in
 *
 * Return:
 *	first character position after the delimiter match
 * -----------------------------------------------
 */

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
GBLREF	boolean_t	badchar_inhibit;

int4 op_fnfind(mval *src, mval *del, mint first, mval *dst)
{
	mint 	result;
	char 	*match, *srcptr, *srctop;
	int 	match_res, bytelen, skip, srclen, numpcs;

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);

	if (first > 0)
		first--;
	else
		first = 0 ;

	if (del->str.len == 0)
		result = first + 1 ;
	else if (src->str.len <= first)
		result = 0 ;
	else
	{
		if (MV_IS_SINGLEBYTE(src) && badchar_inhibit)
		{	/* If BADCHARs are to be checked, matchb() shouldn't be used even if the source is entirely single byte */
			numpcs = 1;
			match = (char *)matchb(del->str.len, (uchar_ptr_t)del->str.addr,
				src->str.len - first, (uchar_ptr_t)src->str.addr + first, &match_res, &numpcs);
			result = match_res ? first + match_res : 0;
		} else
		{	/* either the string contains multi-byte characters or BADCHAR check is required */
			result = 0;
			srcptr = src->str.addr;
			srctop = srcptr + src->str.len;
			for (skip = first; (skip > 0 && (srcptr < srctop)); skip--)
			{	/* advance the string to the character position 'first' */
				if (!UTF8_VALID(srcptr, srctop, bytelen) && !badchar_inhibit)
					utf8_badchar(0, (unsigned char *)srcptr, (unsigned char *)srctop, 0, NULL);
				srcptr += bytelen;
			}
			if (skip <= 0)
			{
				srclen = (int)(srctop - srcptr);
				numpcs = 1;
				match = (char *)matchc(del->str.len, (uchar_ptr_t)del->str.addr,
					srclen, (uchar_ptr_t)srcptr, &match_res, &numpcs);
				result = match_res ? first + match_res : 0;
			}
		}
	}
	MV_FORCE_MVAL(dst, result);
	return result ;
}
#endif /* UNICODE_SUPPORTED */

int4 op_fnzfind(mval *src, mval *del, mint first, mval *dst)
{
	mint	result;
	char	*match;
	int	match_res, numpcs;

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);

	if (first > 0)
		first--;
	else
		first = 0 ;

	if (del->str.len == 0)
		result = first + 1 ;
	else if (src->str.len > first)
	{
		numpcs = 1;
		match = (char *)matchb(del->str.len, (uchar_ptr_t)del->str.addr,
				src->str.len - first, (uchar_ptr_t)src->str.addr + first, &match_res, &numpcs);
		result = match_res ? first + match_res : 0;
	} else
		result = 0 ;
	MV_FORCE_MVAL(dst, result);
	return result ;
}
