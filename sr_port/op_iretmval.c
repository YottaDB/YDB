/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"

void op_iretmval(mval *v)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	(TREF(ind_result_sp))--;
	assert(TREF(ind_result_sp) < TREF(ind_result_top));
	assert(TREF(ind_result_sp) >= TREF(ind_result_array));
	MV_FORCE_DEFINED(v);
	**(TREF(ind_result_sp)) = *v;
	(*(TREF(ind_result_sp)))->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	op_unwind();
}
