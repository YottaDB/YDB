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
#include "opcode.h"

GBLREF triple *curr_fetch_trip, *curr_fetch_opr;
GBLREF int4 curr_fetch_count;

void start_fetches(opctype op)
{
	if (!curr_fetch_count)
		curr_fetch_trip->opcode = (curr_fetch_trip->opcode == OC_LINEFETCH) ? OC_LINESTART : OC_NOOP;
	else
	{
		curr_fetch_trip->operand[0] = put_ilit(curr_fetch_count);
		curr_fetch_count = 0;
	}
	curr_fetch_opr = curr_fetch_trip = newtriple(op);
	return;
}
