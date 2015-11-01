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

GBLDEF triple *curr_fetch_trip, *curr_fetch_opr;
GBLDEF int4 curr_fetch_count;

oprtype put_mvar(mident *x)
{
	triple *ref,*fetch;
	mvar *var;

	ref = newtriple(OC_VAR);
	ref->operand[0].oprclass = MVAR_REF;
	ref->operand[0].oprval.vref = var = get_mvaddr(x);
	if (var->last_fetch != curr_fetch_trip)
	{
		fetch = newtriple(OC_PARAMETER);
		curr_fetch_opr->operand[1] = put_tref(fetch);
		fetch->operand[0] = put_ilit(var->mvidx);
		curr_fetch_count++;
		curr_fetch_opr = fetch;
		var->last_fetch = curr_fetch_trip;
	}
	ref->destination.oprclass = TVAR_REF;
	ref->destination.oprval.temp = var->mvidx;
	return put_tref(ref);

}
