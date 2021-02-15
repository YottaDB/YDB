/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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
#include "cmidef.h"
#include "gvcmz.h"

GBLREF bool		remlkreq;
GBLREF unsigned short	lks_this_cmd;

error_def(ERR_BADLOCKNEST);

void op_lkinit(void)
{
	if (lks_this_cmd)
	{
		lks_this_cmd = 0;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_BADLOCKNEST);
	}
	if (remlkreq)
	{
		gvcmz_clrlkreq();
		remlkreq = FALSE;
	}
}
