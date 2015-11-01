/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

void op_lkinit(void)
{
	if (remlkreq)
	{
		gvcmz_clrlkreq();
		remlkreq = FALSE;
	}
	lks_this_cmd = 0;
}
