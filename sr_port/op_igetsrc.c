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

void op_igetsrc(mval *v)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	(TREF(ind_source_sp))--;
	assert(TREF(ind_source_sp) < TREF(ind_source_top));
	assert(TREF(ind_source_sp) >= TREF(ind_source_array));
	*v = **(TREF(ind_source_sp));
	v->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	return;
}
