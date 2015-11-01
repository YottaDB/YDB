/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

void op_fnzjobexam(mval *prelimSpec, mval *finalSpec)
{
	MV_FORCE_STR(prelimSpec);
	jobexam_process(prelimSpec, finalSpec);
	return;
}
