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
#include "stringpool.h"
#include "reinit_compilation_externs.h"
#include "gcol_list.h"
#include "gtm_threadgbl.h"


GBLREF spdesc 		stringpool;

void reinit_compilation_externs(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	glist_clear_arrays(&stringpool);
	stringpool.free = stringpool.base;
	mcfree();
}
