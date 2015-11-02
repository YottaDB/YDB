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

GBLREF triple	*curr_fetch_trip, *curr_fetch_opr;
GBLREF int4	curr_fetch_count;
GBLREF mvax *mvaxtab,*mvaxtab_end;

void start_for_fetches (void)
{
	triple	*ref1, *ref2, *fetch_trip;
	int	fetch_count, index, idiff;
	mvax	*idx;

	fetch_trip = curr_fetch_trip;
	fetch_count = curr_fetch_count;
	start_fetches (OC_FETCH);
	ref1 = fetch_trip;
	ref2 = curr_fetch_trip;
	idx = mvaxtab;
	while (ref1->operand[1].oprclass)
	{
		assert (ref1->operand[1].oprclass == TRIP_REF);
		ref1 = ref1->operand[1].oprval.tref;
		assert (ref1->opcode == OC_PARAMETER);
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
				assert (idx->last);
				idx = idx->last;
				idiff++;
			}
			else
			{
				assert (idx->next);
				idx = idx->next;
				idiff--;
			}
		}
		assert (idx->mvidx == index);
		idx->var->last_fetch = curr_fetch_trip;
	}
	curr_fetch_count = fetch_count;
	curr_fetch_opr = ref2;
}
