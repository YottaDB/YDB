/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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
#include "opcode.h"

oprtype put_mvar(mident *x)
{
	triple *ref,*fetch;
	mvar *var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ref = newtriple(OC_VAR);
	ref->operand[0].oprclass = MVAR_REF;
	ref->operand[0].oprval.vref = var = get_mvaddr(x);
	if ((var->last_fetch != (TREF(fetch_control)).curr_fetch_trip) && !TREF(discard))
	{	/* This block is identical to one in m_new */
		fetch = newtriple(OC_PARAMETER);
		(TREF(fetch_control)).curr_fetch_opr->operand[1] = put_tref(fetch);
		fetch->operand[0] = put_ilit(var->mvidx);
		((TREF(fetch_control)).curr_fetch_count)++;
		(TREF(fetch_control)).curr_fetch_opr = fetch;
		var->last_fetch = (TREF(fetch_control)).curr_fetch_trip;
	}
	ref->destination.oprclass = TVAR_REF;
	ref->destination.oprval.temp = var->mvidx;
	return put_tref(ref);

}
