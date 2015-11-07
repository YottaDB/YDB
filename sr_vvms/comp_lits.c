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
#include "compiler.h"
#include <rtnhdr.h>
#include "mdq.h"
#include "stringpool.h"

GBLREF mliteral 	literal_chain;
GBLREF spdesc 		stringpool;
GBLREF unsigned short 	source_name_len;
GBLREF mident		routine_name;

GBLDEF uint4 		lits_size;

void comp_lits(rhead)
rhdtyp *rhead;
{
	uint4 offset;
	mliteral *p;

	offset = stringpool.free - stringpool.base;
	offset += PADLEN(offset, NATIVE_WSIZE);
	rhead->src_full_name.len = source_name_len;
	rhead->src_full_name.addr = (char *)offset;
	offset += source_name_len;
	offset += PADLEN(offset, NATIVE_WSIZE);
	rhead->routine_name.len = routine_name.len;
	rhead->routine_name.addr = (char *)offset;
	offset += routine_name.len;
	offset += PADLEN(offset, NATIVE_WSIZE);
	dqloop(&literal_chain, que, p)
		if (p->rt_addr < 0)
		{
			p->rt_addr = offset;
			offset += SIZEOF(mval);
		}
	lits_size = offset;
}
