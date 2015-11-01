/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
int4 op_fnfind(mval *src, mval *del, mint first, mval *dst)
{
	mint result;
	char *match;
	int match_res;

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);

	if (first > 0)
		first--;
	else
		first = 0 ;

	if (del->str.len == 0)
	{
		result = first + 1 ;
	}
	else if (src->str.len - first > 0 )
	{
		match = (char *)matchc( del->str.len, (uchar_ptr_t)del->str.addr,
				src->str.len - first, (uchar_ptr_t)src->str.addr + first,
				&match_res);
		result = ( !match_res ? match - src->str.addr + 1 : 0 ) ;
	}
	else
	{
		result = 0 ;
	}
	MV_FORCE_MVAL(dst, result);
	return result ;
}
