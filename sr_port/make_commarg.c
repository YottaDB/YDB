/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#ifdef DEBUG
LITREF octabstruct oc_tab[];
#endif

void make_commarg(oprtype *x,mint ind)
{
	triple *ref;

	assert(x->oprclass == TRIP_REF);
	ref = x->oprval.tref;
	if (ref->opcode != OC_INDGLVN)
	{
		assert((OC_COMVAL == ref->opcode) || (OC_COMINT == ref->opcode) || (OC_COBOOL == ref->opcode));
		dqdel(ref,exorder);
		assert(ref->operand[0].oprclass == TRIP_REF);
		ref = ref->operand[0].oprval.tref;
		assert(ref->opcode == OC_INDGLVN);
	}
	ref->opcode = OC_COMMARG;
	ref->operand[1] = put_ilit(ind);
	return;
}
