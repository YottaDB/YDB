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

#include "op.h"
#include "matchc.h"
#include "fnpc.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	gtm_utf8_mode;

/* --------------------------------------------------------------------
 * NOTE: This module is a near copy of sr_port/op_fnzpiece.c differing
 * only in that it calls "matchc" instead of "matchb" to do matching.
 * --------------------------------------------------------------------
 */

/*
 * -----------------------------------------------
 * op_fnpiece()
 * MUMPS $Piece function for unicode
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
void op_fnpiece(mval *src, mval *del, int first, int last, mval *dst)
{
	int		piece_cnt, del_len, src_len;
	char		*del_str, *src_str;
	char		*match_start;
	int		match_res, int_del;
	delimfmt	unichar;

	assert(gtm_utf8_mode);
	if (--first < 0)
		first = 0;
	if ((piece_cnt = last - first) < 1)
	{
		MV_INIT_STRING(dst, 0, NULL);
		return;
	}
	MV_FORCE_STR(src);
	MV_FORCE_STR(del);
	/* See if we can take a short cut to op_fnp1. If so, the delimiter value needs to be reformated. */
	if (1 == piece_cnt && 1 == MV_FORCE_LEN(del))
        { /* Both valid chars of char_len=1 and single byte invalid chars get the fast path */
		unichar.unichar_val = 0;
		assert(SIZEOF(unichar.unibytes_val) >= del->str.len);
		memcpy(unichar.unibytes_val, del->str.addr, del->str.len);
		op_fnp1(src, unichar.unichar_val, last, dst); /* Use last as it is unmodified */
		return;
	}
	src_len = src->str.len;
	src_str = src->str.addr;
	del_len = del->str.len;
	del_str = del->str.addr;
	if (first)
	{
		match_start = (char *)matchc(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res, &first);
		if (0 == match_res)
		{
			MV_INIT_STRING(dst, 0, NULL);
			return;
		}
		src_len -= INTCAST(match_start - src_str);
		src_str = match_start;
	} else
		match_start = src_str;
	src_str = (char *)matchc(del_len, (uchar_ptr_t)del_str, src_len, (uchar_ptr_t)src_str, &match_res, &piece_cnt);
	if (0 != match_res)
		src_str -= del_len;
	MV_INIT_STRING(dst, INTCAST(src_str - match_start), match_start);
	return;
}
