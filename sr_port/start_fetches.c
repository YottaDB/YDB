/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cmd_qlf.h"
#include "compiler.h"
#include "opcode.h"

GBLREF command_qualifier	cmd_qlf;
GBLREF triple			*curr_fetch_trip, *curr_fetch_opr;
GBLREF int4			curr_fetch_count;
GBLREF boolean_t		cur_line_entry;	/* TRUE if control can reach this line in a -NOLINE_ENTRY compilation */

void start_fetches(opctype op)
{
	if (!curr_fetch_count &&
		((OC_LINEFETCH != curr_fetch_trip->opcode) || (cmd_qlf.qlf & CQ_LINE_ENTRY) || !cur_line_entry))
	{
		if (OC_LINEFETCH == curr_fetch_trip->opcode)
			curr_fetch_trip->opcode = OC_LINESTART;
		else
			curr_fetch_trip->opcode = OC_NOOP;
	} else
	{
		curr_fetch_trip->operand[0] = put_ilit(curr_fetch_count);
		curr_fetch_count = 0;
	}
	curr_fetch_opr = curr_fetch_trip = newtriple(op);
	return;
}
