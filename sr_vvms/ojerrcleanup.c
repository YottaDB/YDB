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
#include "ast.h"
#include "job.h"

GBLREF	short		astq_dyn_avail;
GBLREF	bool		ojtimeout;
GBLREF	short		ojpchan;
GBLREF	short		ojcchan;
GBLREF	int4		ojcpid;
GBLREF	short		ojastq;

void ojerrcleanup ()
{
	sys$setast(DISABLE);
	if (FALSE == ojtimeout)
	{
		sys$cantim(&(ojtimeout), 0);
		ojtimeout = TRUE;
	}
	if (0 != ojpchan)
	{
		sys$dassgn(ojpchan);
		ojpchan = 0;
	}
	astq_dyn_avail += ojastq;
	sys$setast(ENABLE);
	ojastq = 0;
	if (0 != ojcchan)
	{
		sys$dassgn(ojcchan);
		ojcchan = 0;
	}
	if (0 != ojcpid)
	{
		sys$delprc(ojcpid);
		ojcpid = 0;
	}
	return;
}
