/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gt_timer.h"
#include "gtcm_shutdown_ast.h"

GBLREF bool cm_shutdown;

void gtcm_shutdown_ast(TID tid, int4 len, char *data)
{
	ASSERT_IS_LIBGNPSERVER;
	cm_shutdown = TRUE;
}
