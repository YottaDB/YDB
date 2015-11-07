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

#include <ssdef.h>

#include "ast.h"
#include "job.h"

GBLREF	short		astq_dyn_avail;
GBLREF	bool		ojtimeout;
GBLREF	short		ojpchan;
GBLREF	short		ojcchan;
GBLREF	int4		ojcpid;
GBLREF	short		ojastq;

void ojcleanup(void)
{
	unsigned int 	ast_stat, status;

	ast_stat = sys$setast(DISABLE);
	if (FALSE == ojtimeout)
	{
		if (SS$_NORMAL != (status = sys$cantim(&(ojtimeout), 0)))
			rts_error(VARLSTCNT(1) status);
		ojtimeout = TRUE;
	}
	if (0 != ojpchan)
	{
		if (SS$_NORMAL != (status = sys$dassgn(ojpchan)))
			rts_error(VARLSTCNT(1) status);
		ojpchan = 0;
	}
	astq_dyn_avail += ojastq;
	if (SS$_WASSET == ast_stat)
		ENABLE_AST;
	ojastq = 0;
	if (0 != ojcchan)
	{
		if (SS$_NORMAL != (status = sys$dassgn(ojcchan)))
			rts_error(VARLSTCNT(1) status);
		ojcchan = 0;
	}
	if (ojcpid != 0)
	{
		if (SS$_NORMAL != (status = sys$delprc(ojcpid)))
			rts_error(VARLSTCNT(1) status);
		ojcpid = 0;
	}
	return;
}
