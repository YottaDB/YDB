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
#include "efn.h"
#include "timers.h"
#include <ssdef.h>

GBLREF short astq_dyn_alloc;
GBLREF short astq_dyn_avail;
GBLREF short astq_dyn_min;

bool ast_get_static(needed)
int needed;
{
	int4		pause[2];

	if ((astq_dyn_alloc - needed) < astq_dyn_min)
		return FALSE;
	astq_dyn_alloc -= needed;
	astq_dyn_avail -= needed;
	pause[0] = TIM_AST_WAIT;
	pause[1] = -1;
	while (astq_dyn_avail < 0 )
	{	if (sys$setimr(efn_immed_wait, &pause, 0, 0, 0) == SS$_NORMAL)
		{	sys$synch(efn_immed_wait, 0);
		}
	};
	return TRUE;
}
