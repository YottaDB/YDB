/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "mdq.h"
#include "stringpool.h"
#include "rtnhdr.h"
#include "copy.h"
#include "obj_file.h"

GBLREF mliteral	literal_chain;
GBLREF spdesc	stringpool,indr_stringpool;

void indir_lits(ihdtyp *ihead)
{
	int 		i, j, x, pad_len;
	int4		long_temp;
	mliteral	*p;
	static int	nulls = 0;

	i = indr_stringpool.free - indr_stringpool.base;
	emit_immed((char *)indr_stringpool.base, i);
	pad_len = (x = i & 3) ? (4 - x) : 0;	/* Length to pad to put instructions on even bdy */
	if (pad_len)
		emit_immed((char *)&nulls, pad_len);	/* Pad out with extraneous info */
	j = i + sizeof(ihdtyp) + pad_len;
	PUT_LONG(&(ihead->fixup_vals_off), j);
	PUT_ZERO(ihead->fixup_vals_num);
	dqloop(&literal_chain, que, p)
		if (p->rt_addr < 0)
		{
			/* ihead->fixup_vals_num++; */
			GET_LONG(long_temp, &(ihead->fixup_vals_num));
			long_temp++;
			PUT_LONG(&(ihead->fixup_vals_num), long_temp);
			p->rt_addr = (int) stringpool.free;
			p->v.str.addr =  (char *)(p->v.str.addr - (char *)indr_stringpool.base + sizeof(ihdtyp));
			emit_immed((char *)&p->v, sizeof(mval));
		}
}
