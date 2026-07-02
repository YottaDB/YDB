/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stringpool.h"
#include "gtmmsg.h"
#include "op.h"

GBLREF spdesc stringpool;
#define MAX_MSG_SIZE 256

void op_fnzm(mint x, mval *v)
{
	int4	l_x;
	mstr	msg;

	l_x = x;
	v->mvtype = 0;
	v->str.len = 0; /* so stp_gcol can free up space currently occupied by this to-be-overwritten mval */
	ENSURE_STP_FREE_SPACE(MAX_MSG_SIZE);
	v->str.addr = (char *)stringpool.free;
	msg.addr = (char*) stringpool.free;
	msg.len = MAX_MSG_SIZE;

	gtm_getmsg(l_x, &msg);
	stringpool.free += msg.len;
	v->str.len = msg.len;
	v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
}
