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
#include "gtm_string.h"
#include "compiler.h"
#include "mdq.h"
#include "mmemory.h"
#include "op.h"
#include "opcode.h"
#include "fullbool.h"
#include "stringpool.h"
#include "toktyp.h"
#include "flt_mod.h"

LITREF mval		literal_minusone, literal_one, literal_zero;
LITREF octabstruct	oc_tab[];

error_def(ERR_NUMOFLOW);


inline void ex_arithlit(triple *t)
{
	mval		*v, *v0, *v1;
	oprtype		*i;
	uint		j;
	triple		*t0, *t1;
	opctype		c;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	c = t->opcode;
	for (i = t->operand, j = 0; ARRAYTOP(t->operand) > i; i++, j++)
	{
		if (OC_LIT != t->operand[j].oprval.tref->opcode)
			break;
	}
	if (ARRAYTOP(t->operand) > i)
		return;
	for (t0 = t->operand[0].oprval.tref; TRIP_REF == t0->operand[0].oprclass; t0 = t0->operand[0].oprval.tref)
		dqdel(t0, exorder);
	for (t1 = t->operand[1].oprval.tref; TRIP_REF == t1->operand[0].oprclass; t1 = t1->operand[0].oprval.tref)
		dqdel(t1, exorder);
	v0 = &t0->operand[0].oprval.mlit->v;
	MV_FORCE_NUMD(v0);
	v1 = &t1->operand[0].oprval.mlit->v;
	MV_FORCE_NUMD(v1);
	if (!(MV_NM & v1->mvtype) || !(MV_NM & v0->mvtype))
	{	/* if we don't have a useful number we can't do useful math */
		TREF(last_source_column) += (TK_EOL == TREF(director_token)) ? -2 : 2;	/* improve hints */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NUMOFLOW);
		assert(TREF(rts_error_in_parse));
		return;
	}
	v = (mval *)mcalloc(SIZEOF(mval));
	switch (c)
	{
	case OC_ADD:
		op_add(v0, v1, v);
		break;
	case OC_DIV:
	case OC_IDIV:
	case OC_MOD:
		if (!(MV_NM & v1->mvtype) || (0 != v1->m[1]))
		{
			if (OC_DIV == c)
				op_div(v0, v1, v);
			else if (OC_MOD == c)
				flt_mod(v0, v1, v);
			else
				op_idiv(v0, v1, v);
		} else				/* divide by literal 0 is a technique so let it go to run time*/
			v = NULL;		/* flag value to get out of nested switch */
		break;
	case OC_EXP:
		op_exp(v0, v1, v);
		if ((1 == v->sgn) && (MV_INT & v->mvtype) && (0 == v->m[0]))
			v = NULL;		/* flag value from op_exp indicates DIVZERO, so leave to run time */
		break;
	case OC_MUL:
		op_mul(v0, v1, v);
		break;
	case OC_SUB:
		op_sub(v0, v1, v);
		break;
	default:
		assertpro(FALSE && t1->opcode);
		break;
	}
	RETURN_IF_RTS_ERROR;
	if ((NULL == v) || (!v->mvtype))
		return;		/* leave divide by zero or missing mvtype from NUMOFLOW to cause run time errors */
	unuse_literal(v0);	/* drop original literals only after deciding whether to defer */
	unuse_literal(v1);
	dqdel(t0, exorder);
	dqdel(t1, exorder);
	n2s(v);
	s2n(v);			/* compiler must leave literals with both numeric and string */
	t->opcode = OC_LIT;	/* replace the original operator triple with new literal */
	put_lit_s(v, t);
	t->operand[1].oprclass = NO_REF;
	return;
}
