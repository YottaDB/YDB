/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
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
#include "resolve_lab.h"

error_def(ERR_LABELMISSING);

void resolve_lab(mtreenode *node, void *arg)
{
	int	*errknt = arg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!node->lab.ml)
	{
		(*errknt)++;
		stx_error(ERR_LABELMISSING, 2, node->lab.mvname.len, node->lab.mvname.addr);
		TREF(source_error_found) = 0;
	}
	return;
}
