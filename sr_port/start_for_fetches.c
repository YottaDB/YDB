/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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

GBLREF int4	curr_fetch_count;
GBLREF mvax	*mvaxtab;
GBLREF triple	*curr_fetch_opr, *curr_fetch_trip;

/* When in the body of a FOR loop, we need to maintain the binding for the control variable.
 * If the action of a command (or function) can alter the symbol table, e.g. BREAK or NEW,
 * it should call this routine in preference to start_fetches when it detects that it's in the
 * body of a FOR. While start_fetches just starts a new fetch, this copies the arguments of the prior
 * fetch to the new fetch because there's no good way to tell which one is for the control variable
 */
void start_for_fetches(void)
{
	triple	*fetch_trip, *ref1, *ref2;
	int	fetch_count, idiff, index;
	mvax	*idx;

	fetch_trip = curr_fetch_trip;
	fetch_count = curr_fetch_count;
	start_fetches(OC_FETCH);
	ref1 = fetch_trip;
	ref2 = curr_fetch_trip;
	idx = mvaxtab;
	while (ref1->operand[1].oprclass)
	{
		assert(ref1->operand[1].oprclass == TRIP_REF);
		ref1 = ref1->operand[1].oprval.tref;
		assert(ref1->opcode == OC_PARAMETER);
		ref2->operand[1] = put_tref (newtriple (OC_PARAMETER));
		ref2 = ref2->operand[1].oprval.tref;
		ref2->operand[0] = ref1->operand[0];
		assert(ref2->operand[0].oprclass == TRIP_REF &&
			ref2->operand[0].oprval.tref->opcode == OC_ILIT);
		index = ref2->operand[0].oprval.tref->operand[0].oprval.ilit;
		idiff = index - idx->mvidx;
		while (idx->mvidx != index)
		{
			if (idiff < 0)
			{
				assert(idx->last);
				idx = idx->last;
				idiff++;
			} else
			{
				assert(idx->next);
				idx = idx->next;
				idiff--;
			}
		}
		assert(idx->mvidx == index);
		idx->var->last_fetch = curr_fetch_trip;
	}
	curr_fetch_count = fetch_count;
	curr_fetch_opr = ref2;
}
