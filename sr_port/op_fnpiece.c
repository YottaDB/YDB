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
#include "op.h"
#include "matchc.h"

/*
 * -----------------------------------------------
 * op_fnpiece()
 * MUMPS Piece function
 *
 * Arguments:
 *	src	- Pointer to Source string mval
 *	del	- Pointer to delimiter mval
 *	first	- starting piece number
 *	last	- last piece number
 *	dst	- destination buffer to save the piece in
 *
 * Return:
 *	none
 * -----------------------------------------------
 */
void op_fnpiece(mval *src,mval *del,int first,int last,mval *dst, boolean_t srcisliteral)
{
	int	piece_cnt, ofirst;
	int	del_len, src_len;
	char	*del_str, *src_str, *tmp_str;
	char	*match_start;
	int	match_res;

	ofirst = first;
	if (--first < 0)
		first = 0;

	if ((piece_cnt = last - first) < 1)
		goto isnull;

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);

	/* See if we can take a short cut to op_fnp1 */
	if (1 == del->str.len && ofirst == last)
	{
		op_fnp1(src, (int)(*del->str.addr), ofirst, dst, srcisliteral);
		return;
	}
	src_len = src->str.len;
	src_str = src->str.addr;
	del_len = del->str.len;
	del_str = del->str.addr;
	while (first--)
	{
		tmp_str = (char *)matchc(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res);
		src_len -= (tmp_str - src_str);
		src_str = tmp_str;
		if (match_res)
			goto isnull;
	}
	match_start = src_str;
	while (piece_cnt--)
	{
		tmp_str = (char *)matchc(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res);
		src_len -= (tmp_str - src_str);
		src_str = tmp_str;
		if (match_res)
			goto no_end_del;
	}

	src_str -= del_len;
no_end_del:
	dst->str.addr = match_start;
	dst->str.len = src_str - match_start;
done:
	dst->mvtype = MV_STR;
	return;

isnull:
	dst->str.len = 0;
	goto done;
}
