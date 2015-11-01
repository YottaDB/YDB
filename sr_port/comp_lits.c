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
#include "compiler.h"
#include "rtnhdr.h"
#include "mdq.h"
#include "stringpool.h"

GBLREF mliteral literal_chain;
GBLREF spdesc stringpool;
GBLREF unsigned short source_name_len;

GBLDEF uint4 lits_size, lit_addrs;

void comp_lits(rhead)
rhdtyp *rhead;
{
	uint4 offset, cnt;
	uint4  align_pad;
	mliteral *p;

	offset = stringpool.free - stringpool.base;
	rhead->src_full_name.len = source_name_len;
	rhead->src_full_name.addr = (char *)offset;
	offset += source_name_len;
	align_pad = offset & 3;
	if (align_pad)
		offset += (4 - align_pad);

	cnt = 0;
	dqloop(&literal_chain, que, p)
		if (p->rt_addr < 0)
		{
			p->rt_addr = offset;
			offset += sizeof(mval);
			if (p->v.str.len)
				cnt++;
		}
	lits_size = offset;
	lit_addrs = cnt;
}
