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
#include "op.h"

void op_iretmval(mval *v, mval *dst)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	*dst = *v;
	dst->mvtype &= ~MV_ALIASCONT;		/* Make sure alias container property does not pass */
	op_unwind();
}
