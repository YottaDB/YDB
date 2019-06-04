/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stringpool.h"
#include "stack_frame.h"
#include "getzposition.h"

GBLREF spdesc		stringpool;

void getzposition (mval *v)
{
	ENSURE_STP_FREE_SPACE(MAX_ENTRYREF_LEN);
	v->mvtype = MV_STR;
	v->str.addr = (char *) stringpool.free;
	stringpool.free = get_symb_line(stringpool.free, MAX_ENTRYREF_LEN, 0, 0);
	v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
	return;
}
