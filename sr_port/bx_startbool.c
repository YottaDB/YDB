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
 * This function finds the leftmost operand which will not be relocated. If it's an impure boolean,
 * that just means the operation itself: we can't short-circuit any of the operands of e.g. 'less than', since we need both
 * to figure out which is greater. That leaves directly nested booleans. This logic steps down to the right
 * if there's a side effect somewhere in that tree; otherwise it knows that it is safe to start the boolchain earlier.
 * Boolinit must be placed before all jmp-creating ops which will not be relocated by a parent SEBOOL.
 * In general, it is better to place the boolinit later if possible to limit nested boolchains and reduce temp usage, but
 * safer to put the boolinit earlier since jmp-placement is independent of it (and if we place it after any jmps we are in trouble.
 * The logic also safely steps past any OC_COMs.
 */
{
	triple		*t1, *t2, *bitrip;

	for (t1 = t; ; t1 = t2)
	{
		assert(TRIP_REF == t1->operand[0].oprclass);
		assert(OCT_BOOL & oc_tab[t1->opcode].octype);

		t2 = t1->operand[0].oprval.tref;
		if (!(OCT_BOOL & oc_tab[t2->opcode].octype))
			break;

		/* The idea here is that if we hit a side-effect boolean, we're done.
		 * ASSUMES: All directly-nested jmp-generating operands are relocated to right before the
		 * boolean, in-order, recursively. This assumption is fulfilled by bx_sboolop.
		 */
		if ((OCT_SE & oc_tab[t1->opcode].octype) && !(OCT_UNARY & oc_tab[t1->opcode].octype))
			break;
	}
	bitrip = maketriple(OC_BOOLINIT);
	DEBUG_ONLY(bitrip->src = t->src);
	dqrins(t1, exorder, bitrip);
	return bitrip;
}
