/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"

mval *op_igetdst(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	return TREF(ind_result);	/* see comment in comp_init.c */
}
