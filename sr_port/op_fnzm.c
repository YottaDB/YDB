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
#include "stringpool.h"
#include "gtmmsg.h"
#include "op.h"

GBLREF spdesc stringpool;
#define MAX_MSG_SIZE 256

void op_fnzm(mint x,mval *v)
{
	int4 l_x;
	mstr msg;

	l_x = x;
	v->mvtype = MV_STR;
	if (stringpool.top - stringpool.free < MAX_MSG_SIZE)
		stp_gcol(MAX_MSG_SIZE);
	v->str.addr = (char *)stringpool.free;
	msg.addr = (char*) stringpool.free;
	msg.len = MAX_MSG_SIZE;

	gtm_getmsg(l_x, &msg);
	stringpool.free += msg.len;
	v->str.len = msg.len;
}
