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

/* --------------------------------------------------------------------
 * NOTE: This module is a near copy of sr_unix/op_fnpiece.c differing
 * only in that it calls "matchb" instead of "matchc" to do matching.
 * --------------------------------------------------------------------
 */

/*
 * -----------------------------------------------
 * op_fnzpiece()
 * MUMPS $ZPIece function
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
void op_fnzpiece(mval *src, mval *del, int first, int last, mval *dst)
{
	int		piece_cnt, del_len, src_len;
	char		*del_str, *src_str;
	char		*match_start;
	int		match_res;
	delimfmt	unichar;

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
	if ((1 == del->str.len) && (1 == piece_cnt))
	{
		unichar.unichar_val = 0;
		unichar.unibytes_val[0] = *del->str.addr;
		op_fnzp1(src, unichar.unichar_val, last, dst);	/* Last is the one that counts as first may be too low */
		return;
	}
	src_len = src->str.len;
	src_str = src->str.addr;
	del_len = del->str.len;
	del_str = del->str.addr;
	if (first)
	{
		match_start = (char *)matchb(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res, &first);
		if (0 == match_res)
		{	/* reached end of input string */
			dst->str.len = 0;
			dst->mvtype = MV_STR;
			return;
		}
		src_len -= INTCAST(match_start - src_str);
		src_str = match_start;
	} else
		match_start = src_str;
	src_str = (char *)matchb(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res, &piece_cnt);
	if (0 != match_res)
		src_str -= del_len;
	dst->str.addr = match_start;
	dst->str.len = INTCAST(src_str - match_start);
	dst->mvtype = MV_STR;
	return;
}
