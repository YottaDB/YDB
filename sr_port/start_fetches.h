/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef START_FETCHES_H_INCLUDED
#define START_FETCHES_H_INCLUDED

#include "cmd_qlf.h"

GBLREF command_qualifier	cmd_qlf;
GBLREF boolean_t		cur_line_entry;	/* TRUE if control can reach this line in a -NOLINE_ENTRY compilation */

#define START_FETCHES(OP) start_fetches(OP)

static inline void start_fetches(opctype op)
{
	boolean_t	noline_entry_point;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If the compilation happens with -NOLINE_ENTRY and control can reach the current M line from an external entry point,
	 * then make sure the current OC_LINEFETCH accounts for ALL local variable references in this frame (not just the
	 * ones seen till now in the parse, but also the ones that are going to follow under this entryref). A simple way
	 * of ensuring this is by setting "(TREF(fetch_control))curr_fetch_count" to 0 in this case. Not doing so can
	 * cause SIG-11 (YDB#901).
	 */
	noline_entry_point = ((OC_LINEFETCH == (TREF(fetch_control)).curr_fetch_trip->opcode)
					&& !(cmd_qlf.qlf & CQ_LINE_ENTRY) && cur_line_entry);
	if (noline_entry_point)
		(TREF(fetch_control)).curr_fetch_count = 0;
	if (!(TREF(fetch_control)).curr_fetch_count && !noline_entry_point)
		(TREF(fetch_control)).curr_fetch_trip->opcode =
			((TREF(fetch_control)).curr_fetch_trip->opcode == OC_LINEFETCH) ? OC_LINESTART : OC_NOOP;
	else
	{
		assert((OC_LINEFETCH == (TREF(fetch_control)).curr_fetch_trip->opcode)
			|| (OC_FETCH == (TREF(fetch_control)).curr_fetch_trip->opcode));
		(TREF(fetch_control)).curr_fetch_trip->operand[0] = put_ilit((TREF(fetch_control)).curr_fetch_count);
		(TREF(fetch_control)).curr_fetch_count = 0;
	}
	(TREF(fetch_control)).curr_fetch_opr = (TREF(fetch_control)).curr_fetch_trip = newtriple(op);
	return;
}

/* The following macro should be used when doing an OC_FETCH other than at the start of a line because
 * it deals with the possibility that there is a preceding FOR, in which case, we use start_for_fetches to
 * be sure the new fetch includes any FOR loop control variable
 */
#define MID_LINE_REFETCH mid_line_refetch()

static inline void mid_line_refetch(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((0 < (TREF(fetch_control)).curr_fetch_count) && (OC_NOOP == (TREF(fetch_control)).curr_fetch_trip->opcode))
		(TREF(fetch_control)).curr_fetch_trip->opcode = OC_FETCH;
	if (TREF(for_stack_ptr) == TADR(for_stack))
		START_FETCHES(OC_FETCH);
	else	/* if in the body of a FOR loop we need to ensure the control variable is refetched */
		start_for_fetches();
	return;
}
#endif
