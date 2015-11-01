#include "mdef.h"
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

#include <stdio.h>
#include "gtm_stdlib.h"
#include "cli.h"
#include "util_spawn.h"


GBLREF char	parm_ary[MAX_PARMS][MAX_LINE];
GBLREF unsigned	parms_cnt;

void util_spawn(void)
{
	char *cmd;

	assert(1 >= parms_cnt);

	if (0 == parms_cnt)
	{
		cmd = GETENV("SHELL");
		if (!cmd)
			cmd = "/bin/sh";
		SYSTEM(cmd);
	}
	else
	{
		SYSTEM(parm_ary [parms_cnt - 1]);
	}
}

