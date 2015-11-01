/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
	int 		size, pad_len;
	int4		long_temp;
	mliteral	*lit;

	size = indr_stringpool.free - indr_stringpool.base;
	emit_immed((char *)indr_stringpool.base, size);
	pad_len = PADLEN(size, SECTION_ALIGN_BOUNDARY);	/* Length to pad to put instructions on even bdy */
	if (pad_len)
		emit_immed(PADCHARS, pad_len);	/* Pad out with extraneous info */
	size += (sizeof(ihdtyp) + pad_len);
	PUT_LONG(&(ihead->fixup_vals_off), size);
	PUT_ZERO(ihead->fixup_vals_num);
	dqloop(&literal_chain, que, lit)
	{
		if (lit->rt_addr < 0)
		{
			/* ihead->fixup_vals_num++; */
			GET_LONG(long_temp, &(ihead->fixup_vals_num));
			long_temp++;
			PUT_LONG(&(ihead->fixup_vals_num), long_temp);
			lit->rt_addr = (int) stringpool.free;
			lit->v.str.addr =  (char *)(lit->v.str.addr - (char *)indr_stringpool.base + sizeof(ihdtyp));
			emit_immed((char *)&lit->v, sizeof(mval));
		}
	}
}
