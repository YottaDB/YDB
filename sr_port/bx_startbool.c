/****************************************************************
 *								*
 * Copyright (c) 2023 Fidelity National Information		*
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
#include "mdq.h"
#include "mmemory.h"
#include "opcode.h"

LITREF octabstruct	oc_tab[];

inline triple *bx_startbool(triple *t)
/* t - a boolean operation of any kind which tail-processing is to convert into a boolchain
 * This function finds the leftmost operand which it is safe to short-circuit. If it's an impure boolean,
 * that just means the operation itself: we can't short-circuit any of the operands of e.g. 'less than', since we need both
 * to figure out which is greater. That leaves directly nested booleans. This logic steps down to the right
 * if there's a side effect somewhere in that tree; otherwise it knows that it is safe to start the boolchain earlier.
 * The logic also safely steps past any OC_COMs.
 */
{
	triple		*t1, *t2, *bitrip;
	for (t1 = t; ; t1 = t2)
	{
		assert(TRIP_REF == t1->operand[0].oprclass);
		/* The idea here is that if we've still got a side effect somewhere in the
		 * right-hand branch, we need to proceed down that branch until either we find a boolean that
		 * doesn't have a right-hand-side-effect (in which case it is fine to short-circuit that one, so
		 * move down its lefthand branch) or we hit a non-boolean. Typically we should hit a cobool, and
		 * our boolinit will be inserted right before it.
		 */
		if (OCT_SE & oc_tab[t1->opcode].octype && !(OCT_UNARY & oc_tab[t1->opcode].octype))
		{
			assert(TRIP_REF == t1->operand[1].oprclass);
			t2 = t1->operand[1].oprval.tref;
		}
		else
			t2 = t1->operand[0].oprval.tref;
		if (!(OCT_BOOL & oc_tab[t2->opcode].octype))
			break;
	}
	/* TODO - still necessary? */
	for (; ; t1 = t2)
	{
		assert(TRIP_REF == t1->operand[0].oprclass);
		t2 = t1->operand[0].oprval.tref;
		if (!(OCT_BOOL & oc_tab[t2->opcode].octype))
			break;
	}
	bitrip = maketriple(OC_BOOLINIT);
	DEBUG_ONLY(bitrip->src = t->src);
	dqrins(t1, exorder, bitrip);
	return bitrip;
}
