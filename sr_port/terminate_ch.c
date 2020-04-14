/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

GBLREF boolean_t	created_core;
GBLREF boolean_t	dont_want_core;
GBLREF int4		exi_condition;

CONDITION_HANDLER(terminate_ch)
{
	gtm_dump();
	TERMINATE;
}
