/****************************************************************
 *      Copyright 2001 Sanchez Computer Associates, Inc.        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;

/*
 * -----------------------------------------------
 * op_fnreverse()
 * MUMPS Reverse String function
 *
 * Arguments:
 *	src	- Pointer to Source string mval
 *	dst	- Pointer to destination mval to save the inverted string
 *
 * Return:
 *	none
 * -----------------------------------------------
 */
void op_fnreverse(mval *src, mval *dst)
{
	int	lcnt;
        char    *in, *out;

        if (stringpool.free + src->str.len > stringpool.top)
                stp_gcol(src->str.len);
	MV_FORCE_STR(src);
        out = (char *)stringpool.free;
        stringpool.free += src->str.len;
        in = src->str.addr + src->str.len * sizeof(char);
	dst->mvtype = MV_STR;
	dst->str.addr = out;
	dst->str.len = src->str.len;
        for (lcnt = src->str.len; lcnt > 0; lcnt--)
                *out++ = *--in;
	return;
}
