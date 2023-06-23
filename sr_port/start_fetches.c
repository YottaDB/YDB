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
	boolean_t	noline_entry_point;

	/* If the compilation happens with -NOLINE_ENTRY and control can reach the current M line from an external entry point,
	 * then make sure the current OC_LINEFETCH accounts for ALL local variable references in this frame (not just the
	 * ones seen till now in the parse, but also the ones that are going to follow under this entryref). A simple way
	 * of ensuring this is by setting "curr_fetch_count" to 0 in this case. Not doing so can cause SIG-11 (YDB#901).
	 */
	noline_entry_point = ((OC_LINEFETCH == curr_fetch_trip->opcode) && !(cmd_qlf.qlf & CQ_LINE_ENTRY) && cur_line_entry);
	if (noline_entry_point)
		curr_fetch_count = 0;
	if (!curr_fetch_count && !noline_entry_point)
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
