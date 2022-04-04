/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef START_FETCHES_H_INCLUDED
#define START_FETCHES_H_INCLUDED

#define START_FETCHES(OP) start_fetches(OP)

static inline void start_fetches(opctype op)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!(TREF(fetch_control)).curr_fetch_count)
		(TREF(fetch_control)).curr_fetch_trip->opcode = ((TREF(fetch_control)).curr_fetch_trip->opcode == OC_LINEFETCH)
			? OC_LINESTART : OC_NOOP;
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
