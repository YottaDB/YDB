/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"


#include "error.h"
#include "gt_timer.h"
#include "util.h"
#include "gv_rundown.h"
#include "print_exit_stats.h"
#include "secshr_db_clnup.h"

GBLREF boolean_t        need_core;
GBLREF boolean_t        created_core;
GBLREF boolean_t	exit_handler_active;

void util_exit_handler()
{
	int	stat;

	if (exit_handler_active)	/* Don't recurse if exit handler exited */
		return;
	exit_handler_active = TRUE;
	cancel_timer(0);		/* Cancel all timers - No unpleasant surprises */
	secshr_db_clnup(NORMAL_TERMINATION);
	gv_rundown();
	print_exit_stats();
	util_out_close();
	if (need_core && !created_core)
		DUMP_CORE;
}
