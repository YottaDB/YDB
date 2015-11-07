/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_stdlib.h"
#include "cli.h"
#include "util_spawn.h"

void util_spawn(void)
{
	char *cmd;
	int  rc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(1 >= TREF(parms_cnt));
	if (0 == TREF(parms_cnt))
	{
		cmd = GETENV("SHELL");
		if (!cmd)
			cmd = "/bin/sh";
		rc = SYSTEM(cmd);
		if (-1 == rc)
			PERROR("system : ");
	} else
	{
		assert(TAREF1(parm_ary, TREF(parms_cnt) - 1));
		assert((char *)-1L != (TAREF1(parm_ary, TREF(parms_cnt) - 1)));
		rc = SYSTEM((TAREF1(parm_ary, TREF(parms_cnt) - 1)));
		if (-1 == rc)
			PERROR("system : ");
	}
}
