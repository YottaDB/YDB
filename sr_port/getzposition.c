/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stringpool.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "getzposition.h"

GBLREF spdesc		stringpool;

void getzposition (mval *v)
{
	if (stringpool.top - stringpool.free < MAX_ENTRYREF_LEN)
		stp_gcol(MAX_ENTRYREF_LEN);
	v->mvtype = MV_STR;
	v->str.addr = (char *) stringpool.free;
	stringpool.free = get_symb_line (stringpool.free, 0, 0);
	v->str.len = (char *) stringpool.free - v->str.addr;
	return;
}
