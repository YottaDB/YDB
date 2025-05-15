/****************************************************************
 *								*
 * Copyright (c) 2020-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "mdq.h"

void bx_insert_oc_andor(opctype andor_opcode, int depth, triple *leftmost[NUM_TRIPLE_OPERANDS])
{
	triple		*ref_parms, *t0, *t1;
	int		opridx;
	uint4		combined_opcode;
#	ifdef DEBUG
	triple		*t2;
	opctype		dbg_andor_opcode;
	int		dbg_depth, dbg_opridx;
#	endif

	for (opridx = 0; opridx < NUM_TRIPLE_OPERANDS; opridx++)
	{
		t0 = maketriple(OC_ANDOR);
		COMBINE_ANDOR_OPCODE_DEPTH_OPRINDX(combined_opcode, andor_opcode, depth, opridx);
		t0->operand[0] = make_ilit((mint)combined_opcode);
		/* It is possible nested invocations of `bx_boolop` (from `bx_tail()` call above) might
		 * have inserted `OC_ANDOR` triples BEFORE the `leftmost[]` point we had noted down above.
		 * In that case we need to go back those triples before inserting the `OC_ANDOR` triple
		 * for this invocation. In a similar fashion as the above OC_ANDOR loop, it is possible
		 * that the above invocation of `bx_tail()` could have inserted `OC_BOOLINIT` triples
		 * BEFORE the `leftmost[]` point we had noted down above. We need to go back those too.
		 * The OC_BOOLINIT go-back needs to happen only for opridx=1.
		 */
		for (t1 = leftmost[opridx];
			((OC_ANDOR == t1->exorder.bl->opcode)
				|| ((0 != opridx) && (OC_BOOLINIT == t1->exorder.bl->opcode)));
			t1 = t1->exorder.bl)
		{	/* Assert that OC_ANDOR triples (if any found) correspond to deeper expressions */
#			ifdef DEBUG
			if (OC_ANDOR == t1->exorder.bl->opcode)
			{
				t2 = t1->exorder.bl->operand[0].oprval.tref;
				assert(OC_ILIT == t2->opcode);
				assert(ILIT_REF == t2->operand[0].oprclass);
				combined_opcode = t2->operand[0].oprval.ilit;
				SPLIT_ANDOR_OPCODE_DEPTH_OPRINDX(combined_opcode, dbg_andor_opcode, dbg_depth, dbg_opridx);
				assert(dbg_depth > depth);
			}
#			endif
		}
		dqins(t1->exorder.bl, exorder, t0);
	}
}
