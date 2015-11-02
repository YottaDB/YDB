/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "toktyp.h"
#include "mmemory.h"

void ex_tail(oprtype *opr)
{
	LITREF octabstruct oc_tab[];
	triple *t, *t1, *t2, *bitrip;
	oprtype *i;
	opctype c;
	unsigned short w;

	assert(TRIP_REF == opr->oprclass);
	t = opr->oprval.tref;
	c = t->opcode;
	w = oc_tab[c].octype;
	if (w & OCT_EXPRLEAF)
		return;
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((TRIP_REF == t->operand[1].oprclass) || (NO_REF == t->operand[1].oprclass));
	if (!(w & OCT_BOOL))
	{
		for (i = t->operand; ARRAYTOP(t->operand) > i; i++)
			if (TRIP_REF == i->oprclass)
				ex_tail(i);
		if ((OC_COMINT == c) && (OC_BOOLINIT == (t1 = t->operand[0].oprval.tref)->opcode))	/* NOTE assignment */
			opr->oprval.tref = t1;
	} else
	{
		for (t1 = t; ; t1 = t2)
		{
			assert(TRIP_REF == t1->operand[0].oprclass);
			t2 = t1->operand[0].oprval.tref;
			if (!(oc_tab[t2->opcode].octype & OCT_BOOL))
				break;
		}
		bitrip = maketriple(OC_BOOLINIT);
		dqins(t1->exorder.bl, exorder, bitrip);
		t2 = t->exorder.fl;
		assert(&t2->operand[0] == opr);
		assert((OC_COMVAL == t2->opcode) || (OC_COMINT == t2->opcode));
		if (OC_COMINT == t2->opcode)
			dqdel(t2, exorder);
		t1 = maketriple(OC_BOOLFINI);
		t1->operand[0] = put_tref(bitrip);
		opr->oprval.tref = bitrip;
		dqins(t, exorder, t1);
		i = (oprtype *)mcalloc(SIZEOF(oprtype));
		bx_tail(t, FALSE, i);
		*i = put_tnxt(t1);
	}
	return;
}
