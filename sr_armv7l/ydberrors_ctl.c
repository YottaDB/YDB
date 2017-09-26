/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "error.h"

LITDEF	err_msg ydberrors[] = {
	{ "QUERY2", "Invalid second argument to $QUERY. Must be -1 or 1.", 0 },
};

LITDEF	int ERR_QUERY2 = 151027722;

GBLDEF	err_ctl ydberrors_ctl = {
	256,
	"YDB",
	&ydberrors[0],
	1};
