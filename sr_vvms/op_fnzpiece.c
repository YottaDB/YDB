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
#include "op.h"
#include "matchc.h"
#include "fnpc.h"

/*
 * -----------------------------------------------
 * op_fnzpiece()
 * MUMPS $ZPIece function (and non-unicode $PIECE)
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
void op_fnzpiece(mval *src, mval *del, int first, int last, mval *dst, boolean_t srcisliteral)
{
	int		piece_cnt, ofirst;
	int		del_len, src_len;
	char		*del_str, *src_str, *tmp_str;
	char		*match_start;
	int		match_res, numpcs;
	delimfmt	unichar;

	ofirst = first;
	if (--first < 0)
		first = 0;

	if ((piece_cnt = last - first) < 1)
	{
		dst->str.len = 0;
		dst->mvtype = MV_STR;
		return;
	}

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);

	/* See if we can take a short cut to op_fnzp1 (need to reformat delim argument) */
	if (1 == del->str.len && ofirst == last)
	{
		unichar.unichar_val = 0;
		unichar.unibytes_val[0] = *del->str.addr;
		op_fnzp1(src, unichar.unichar_val, ofirst, dst, srcisliteral);
		return;
	}
	src_len = src->str.len;
	src_str = src->str.addr;
	del_len = del->str.len;
	del_str = del->str.addr;
	while (first--)
	{
		numpcs = 1;
		tmp_str = (char *)matchb(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res, &numpcs);
		src_len -= (tmp_str - src_str);
		src_str = tmp_str;
		if (0 == match_res)
		{
			dst->str.len = 0;
			dst->mvtype = MV_STR;
			return;
		}
	}
	match_start = src_str;
	while (piece_cnt--)
	{
		numpcs = 1;
		tmp_str = (char *)matchb(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res, &numpcs);
		src_len -= (tmp_str - src_str);
		src_str = tmp_str;
		if (0 == match_res)
			break;
	}
	if (0 != match_res)
		src_str -= del_len;
	dst->str.addr = match_start;
	dst->str.len = src_str - match_start;
	dst->mvtype = MV_STR;
	return;
}
