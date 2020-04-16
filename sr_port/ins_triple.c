/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
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
#include "mdq.h"

void ins_triple(triple *x)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	dqnoop(TREF(curtchain));
	dqrins(TREF(curtchain), exorder, x);
}
