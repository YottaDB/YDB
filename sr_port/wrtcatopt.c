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
#include "opcode.h"
#include "mdq.h"

#define STO_LPX(X) (*lpx && (*lpx < lptop) ? (*++(*lpx) = ((X)->operand[0].oprval.tref->opcode == OC_LIT) ? (X): 0) : 0)
#define NOT_CAT(X,Y) ((X)->operand[(Y)].oprval.tref->opcode != OC_CAT)

void wrtcatopt(triple *r, triple ***lpx, triple **lptop)
{
	triple *ref, *w;

	assert(r->opcode == OC_CAT);
	assert(r->operand[0].oprclass == TRIP_REF);
	assert(r->operand[1].oprclass == TRIP_REF);
	assert(r->operand[0].oprval.tref->opcode == OC_ILIT);
	ref = r->operand[1].oprval.tref;
	r->operand[0].oprclass = r->operand[1].oprclass = NO_REF;
	r->opcode = OC_NOOP;
	for (;;)
	{
		assert(ref->opcode == OC_PARAMETER);
		if (NOT_CAT(ref,0))
		{
			ref->opcode = OC_WRITE;
			STO_LPX(ref);
		}
		else
		{
			wrtcatopt(ref->operand[0].oprval.tref, lpx, lptop);
			ref->operand[0].oprclass = NO_REF;
			ref->opcode = OC_NOOP;
		}
		if (ref->operand[1].oprclass == 0)
			break;
		assert(ref->operand[1].oprclass == TRIP_REF);
		ref->operand[1].oprclass = NO_REF;
		ref = ref->operand[1].oprval.tref;
		}
}
