/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
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
#include "opcode.h"
#include "fullbool.h"

LITREF octabstruct	oc_tab[];

void bx_relop(triple *t, opctype cmp, opctype tst, oprtype *addr)
/* work Boolean relational arguments
 * *t points to the Boolean operation
 * cmp and tst give (respectively) the opcode and the associated jump
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 */
{
	oprtype	*p;
	tbp	*tripbp;
	triple	*bini, *comv, *ref, *ref0, ref1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ref = maketriple(tst);
	ref->operand[0] = put_indr(addr);
	dqins(t, exorder, ref);
	t->opcode = cmp;
	for (p = t->operand ; p < ARRAYTOP(t->operand); p++)
	{	/* Some day investigate whether this is still needed */
		if (TRIP_REF == p->oprclass)
		{
			ex_tail(p);
			RETURN_IF_RTS_ERROR;
			if ((NULL != TREF(boolchain_ptr)) && (OC_COMVAL == (comv = p->oprval.tref)->opcode))
			{	/* RELOP needed a BOOLINIT/BOOLFINI/COMVAL, which now must move to the boolchain */
				assert(TREF(saw_side_effect) && (GTM_BOOL != TREF(gtm_fullbool)));
				assert(OC_BOOLINIT == comv->operand[0].oprval.tref->opcode);
				assert(OC_BOOLFINI == comv->exorder.bl->opcode);
				assert(comv->operand[0].oprval.tref == comv->exorder.bl->operand[0].oprval.tref);
				for (ref = (bini = comv->operand[0].oprval.tref)->exorder.fl; ref != comv; ref = ref->exorder.fl)
				{	/* process to matching end of sequence so to conform with addr adjust at end of bx_boolop */
					if (!(OCT_JUMP & oc_tab[ref->opcode].octype))
						TRACK_JMP_TARGET(ref, ref);
				}
				TRACK_JMP_TARGET(comv, comv);
				dqdelchain(bini->exorder.bl, comv->exorder.fl, exorder);	/* snip out the sequence */
				ref0 = &ref1;							/* anchor it */
				ref0->exorder.fl = bini;
				ref0->exorder.bl = comv;
				ref = (TREF(boolchain_ptr))->exorder.bl;
				dqadd(ref, ref0, exorder);					/* and insert it in new chain */
			}
		}
	}
	return;
}
