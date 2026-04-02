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
#include "compiler.h"
#include "opcode.h"
#include "start_fetches.h"

GBLREF mvax	*mvaxtab;

/* When in the body of a FOR loop, we need to maintain the binding for the control variable.
 * If the action of a command (or function) can alter the symbol table, e.g. BREAK or NEW,
 * it should call this routine in preference to start_fetches when it detects that it's in the
 * body of a FOR. While start_fetches just starts a new fetch, this copies the arguments of the prior
 * fetch to the new fetch because there's no good way to tell which one is for the control variable
 */
void start_for_fetches(void)
{
	triple		*fetch_trip, *ref;
	int		fetch_count, index, opri;
	mvax		*idx;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	fetch_trip = (TREF(fetch_control)).curr_fetch_trip;
	fetch_count = (TREF(fetch_control)).curr_fetch_count;
	START_FETCHES(OC_FETCH);
	(TREF(fetch_control)).curr_fetch_trip->operand[1] = fetch_trip->operand[1];
	idx = mvaxtab;
	for (ref = fetch_trip, opri = 1; (opri < 2) && (TRIP_REF == ref->operand[opri].oprclass); )
	{
		if (OC_PARAMETER == ref->operand[opri].oprval.tref->opcode)
		{
			ref = ref->operand[opri].oprval.tref;
			opri = 0;
		} else
		{
			assert(OC_ILIT == ref->operand[opri].oprval.tref->opcode);
			index = ref->operand[opri].oprval.tref->operand[0].oprval.ilit;

			while (idx->mvidx != index)
			{
				if (index < idx->mvidx)
				{
					assert(idx->last);
					idx = idx->last;
				} else
				{
					assert(idx->next);
					idx = idx->next;
				}
			}
			idx->var->last_fetch = (TREF(fetch_control)).curr_fetch_trip;

			opri++;
		}
	}
	(TREF(fetch_control)).curr_fetch_count = fetch_count;
}
