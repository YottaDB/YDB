/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
	v->umval = (TREF(ind_source))->umval;	/* see comment in comp_init.c */
	v->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	return;
}
