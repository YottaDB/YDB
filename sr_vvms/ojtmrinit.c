/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "efn.h"
#include "iotimer.h"
#include "job.h"
#include "rel_quant.h"

GBLREF	short		astq_dyn_avail;
GBLREF	bool		ojtimeout;
GBLREF	short		ojastq;

void ojtmrinit(int4 *timeout)
{
	int4		addend, onesec;
	unsigned int	ast_stat, status;
	quadword	timblk;

	onesec = -10000000;	/* 1 second delta time */
	addend = 0;
	lib$emul(timeout, &onesec, &addend, &timblk);
	ast_stat = sys$setast(DISABLE);
	while (astq_dyn_avail < 1)
	{
		ENABLE_AST;
		rel_quant();
		DISABLE_AST;
	}
	ojastq++;
	--astq_dyn_avail;
	if (SS$_WASSET == ast_stat)
		sys$setast(ENABLE);
	ojtimeout = FALSE;
	status = sys$setimr(efn_timer, &timblk, &ojtmrrtn, &(ojtimeout), 0);
	if (!(status & 1))
	{
		astq_dyn_avail++;
		rts_error(VARLSTCNT(1) status);
	}
	return;
}
