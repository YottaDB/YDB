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
#include "mdq.h"
#include "mvalconv.h"

LITREF octabstruct oc_tab[];

void coerce(oprtype *a,unsigned short new_type)
{

	mliteral *lit;
	opctype conv, old_op;
	triple *ref, *coerc;

	assert (new_type == OCT_MVAL || new_type == OCT_MINT || new_type == OCT_BOOL);
	assert (a->oprclass == TRIP_REF);
	ref = a->oprval.tref;
	old_op = ref->opcode;
	if (new_type & oc_tab[old_op].octype)
		return;
	if (old_op == OC_COMVAL || old_op == OC_COMINT)
	{
		dqdel(ref,exorder);
		ref = ref->operand[0].oprval.tref;
		old_op = ref->opcode;
		if (new_type & oc_tab[old_op].octype)
			return;
	}
	else if (old_op == OC_LIT && new_type == OCT_MINT)
	{
		lit = ref->operand[0].oprval.mlit;
		if (!(++lit->rt_addr))
			dqdel(lit, que);
		ref->opcode = OC_ILIT;
		ref->operand[0].oprclass = ILIT_REF;
		ref->operand[0].oprval.ilit = MV_FORCE_INT(&(lit->v));
		return;
	}
	if (new_type == OCT_BOOL)
		conv = OC_COBOOL;
	else if (new_type == OCT_MINT)
		conv = OC_COMINT;
	else
		conv = OC_COMVAL;
	coerc = newtriple(conv);
	coerc->operand[0] = put_tref(ref);
	*a = put_tref(coerc);
	return;

}
