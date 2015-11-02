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
#include "gtm_string.h"
#include "compiler.h"
#include "mdq.h"
#include "stringpool.h"
#include <rtnhdr.h>
#include "copy.h"
#include "obj_file.h"
#include "cache.h"

GBLREF mliteral	literal_chain;
GBLREF spdesc	stringpool, indr_stringpool;

void indir_lits(ihdtyp *ihead)
{
	ssize_t		size, lits_pad_len, hdr_pad_len;
	int4		long_temp;
	mliteral	*lit;

	hdr_pad_len = PADLEN(SIZEOF(ihdtyp), NATIVE_WSIZE);
	if (hdr_pad_len) /* additional padding to ihdtyp so that the literal text pool begins at the aligned boundary */
		emit_immed(PADCHARS, (uint4)hdr_pad_len);
	size = indr_stringpool.free - indr_stringpool.base;
	emit_immed((char *)indr_stringpool.base, (uint4)size);
	lits_pad_len = PADLEN(size, NATIVE_WSIZE);	/* Length to pad to put instructions on even bdy */
	if (lits_pad_len)
		emit_immed(PADCHARS, (uint4)lits_pad_len);	/* Pad out with extraneous info */
	size += (SIZEOF(ihdtyp) + hdr_pad_len + lits_pad_len);
	ihead->fixup_vals_off = (int4)(ROUND_UP2(size, NATIVE_WSIZE));
	ihead->fixup_vals_num = 0;
	dqloop(&literal_chain, que, lit)
	{
		if (lit->rt_addr < 0)
		{
			ihead->fixup_vals_num++;
			lit->rt_addr = (INTPTR_T)stringpool.free;
			lit->v.str.addr =  (char *)((lit->v.str.addr - (char *)indr_stringpool.base) +
						SIZEOF(ihdtyp) + hdr_pad_len);
			emit_immed((char *)&lit->v, SIZEOF(mval));
		}
	}
}
