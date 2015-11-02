/****************************************************************
 *								*
 *	Copyright 2006, 2012 Fidelity Information Services, Inc	*
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
#include "op.h"
#include "matchc.h"
#include "fnpc.h"

GBLREF spdesc stringpool;

error_def(ERR_MAXSTRLEN);

/* --------------------------------------------------------------------
 * NOTE: This module is a near copy of sr_unix/op_setpiece.c differing
 * only in that it calls "matchb" instead of "matchc" to do matching.
 * --------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------
 * Set $zpiece procedure.
 * Set pieces first through last to expr.
 *
 * Arguments:
 *	src	- source mval
 *	del	- delimiter string mval
 *	expr	- expression string mval
 *	first	- starting index in source mval to be set
 *	last	- last index
 *	dst	- destination mval where the result is saved.
 *
 * Return:
 *	none
 * ----------------------------------------------------------
 */
void op_setzpiece(mval *src, mval *del, mval *expr, int4 first, int4 last, mval *dst)
{
	size_t		str_len, delim_cnt;
	int 		match_res, len, src_len, first_src_ind, second_src_ind, numpcs;
	unsigned char 	*match_ptr, *src_str, *str_addr, *tmp_str;
	delimfmt	unichar;

	if (0 > --first)
		first = 0;
	assert(last >= first);
	second_src_ind = last - first;
	MV_FORCE_STR(del);
	/* Null delimiter */
	if (0 == del->str.len)
	{
		if (first && src->mvtype)
		{
			/* concat src & expr to dst */
			op_cat(VARLSTCNT(3) dst, src, expr);
			return;
		}
		MV_FORCE_STR(expr);
		*dst = *expr;
		return;
	}
	MV_FORCE_STR(expr);
	if (!MV_DEFINED(src))
	{
		first_src_ind = 0;
		second_src_ind = -1;
	} else
	{
		/* Valid delimiter -  See if we can take a short cut to op_fnzp1. If so, delimiter value needs to be reformated */
		if ((1 == second_src_ind) && (1 == del->str.len))
		{	/* Count of pieces to retrieve is 1 so see what we can do quickly */
			unichar.unichar_val = 0;
			unichar.unibytes_val[0] = *del->str.addr;
			op_setzp1(src, unichar.unichar_val, expr, last, dst);	/* Use "last" since it has not been modified */
			return;
		}
		/* We have a valid src with something in it */
		MV_FORCE_STR(src);
		src_str = (unsigned char *)src->str.addr;
		src_len = src->str.len;
		/* skip all pieces until start one */
		if (first)
		{
			numpcs = first;	/* copy int4 type "first" into "int" type numpcs for passing to matchc */
			match_ptr = matchb(del->str.len, (uchar_ptr_t)del->str.addr, src_len, src_str, &match_res, &numpcs);
			/* Note: "numpcs" is modified above by the function "matchb" to reflect the # of unmatched pieces */
			first = numpcs;	/* copy updated "numpcs" value back into "first" */
		} else
		{
			match_ptr = src_str;
			match_res = 1;
		}
		first_src_ind = INTCAST(match_ptr - (unsigned char *)src->str.addr);
		if (0 == match_res) /* if match not found */
			second_src_ind = -1;
		else
		{
			src_len -= INTCAST(match_ptr - src_str);
			src_str = match_ptr;
			/* skip # delimiters this piece will replace, e.g. if we are setting
			 * pieces 2 - 4, then the pieces 2-4 will be replaced by one piece - expr.
			 */
			match_ptr = matchb(del->str.len, (uchar_ptr_t)del->str.addr, src_len, src_str, &match_res, &second_src_ind);
			second_src_ind = (0 == match_res) ? -1 : INTCAST(match_ptr - (unsigned char *)src->str.addr - del->str.len);
		}
	}
	delim_cnt = (size_t)first;
	/* Calculate total string len. */
	str_len = (size_t)expr->str.len + ((size_t)first_src_ind + ((size_t)del->str.len * delim_cnt));
	/* add len. of trailing chars past insertion point */
	if (0 <= second_src_ind)
		str_len += (size_t)(src->str.len - second_src_ind);
	if (MAX_STRLEN < str_len)
	{
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
		return;
	}
	ENSURE_STP_FREE_SPACE((int)str_len);
	str_addr = stringpool.free;
	/* copy prefix */
	if (first_src_ind)
	{
		memcpy(str_addr, src->str.addr, first_src_ind);
		str_addr += first_src_ind;
	}
	/* copy delimiters */
	while (0 < delim_cnt--)
	{
		memcpy(str_addr, del->str.addr, del->str.len);
		str_addr += del->str.len;
	}
	/* copy expression */
	memcpy(str_addr, expr->str.addr, expr->str.len);
	str_addr += expr->str.len;
	/* copy trailing pieces */
	if (0 <= second_src_ind)
	{
		len = src->str.len - second_src_ind;
		tmp_str = (unsigned char *)src->str.addr + second_src_ind;
		memcpy(str_addr, tmp_str, len);
		str_addr += len;
	}
	assert((str_addr - stringpool.free) == str_len);
	dst->mvtype = MV_STR;
	dst->str.len = INTCAST(str_addr - stringpool.free);
	dst->str.addr = (char *)stringpool.free;
	stringpool.free = str_addr;
	return;
}

