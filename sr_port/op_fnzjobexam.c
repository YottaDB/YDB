/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "op.h"
#include "jobexam_process.h"

void op_fnzjobexam(mval *prelimSpec, mval *fmt, mval *finalSpec)
{
	MV_FORCE_STR(prelimSpec);
	MV_FORCE_STR(fmt);
	jobexam_process(prelimSpec, finalSpec, fmt);
	return;
}
