/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

	assert(opr->oprclass == TRIP_REF);
	t = opr->oprval.tref;
	c = t->opcode;
	w = oc_tab[c].octype;
	if (w & OCT_EXPRLEAF)
		return;
	assert(t->operand[0].oprclass == TRIP_REF);
	assert(t->operand[1].oprclass == TRIP_REF || t->operand[1].oprclass == 0);
	if (!(w & OCT_BOOL))
	{
		for (i = t->operand ; i < ARRAYTOP(t->operand); i++)
			if (i->oprclass == TRIP_REF)
				ex_tail(i);
		if (c == OC_COMINT && (t1 = t->operand[0].oprval.tref)->opcode == OC_BOOLINIT)
			opr->oprval.tref = t1;
	} else
	{
		for (t1 = t ; ; t1 = t2)
		{
			assert(t1->operand[0].oprclass == TRIP_REF);
			t2 = t1->operand[0].oprval.tref;
			if (!(oc_tab[t2->opcode].octype & OCT_BOOL))
				break;
		}
		bitrip = maketriple(OC_BOOLINIT);
		dqins(t1->exorder.bl, exorder, bitrip);
		t2 = t->exorder.fl;
		assert(&t2->operand[0] == opr);
		assert(t2->opcode == OC_COMVAL || t2->opcode == OC_COMINT);
		if (t2->opcode == OC_COMINT)
			dqdel(t2,exorder);
		t1 = maketriple(OC_BOOLFINI);
		t1->operand[0] = put_tref(bitrip);
		opr->oprval.tref = bitrip;
		dqins(t, exorder, t1);
		i = (oprtype *) mcalloc(SIZEOF(oprtype));
		bx_tail(t,(bool) FALSE, i);
		*i = put_tnxt(t1);
	}
	return;
}
