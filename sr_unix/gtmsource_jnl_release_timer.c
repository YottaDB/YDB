/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gt_timer.h"
#include "replgbl.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_threadgbl.h"

void gtmsource_jnl_release_timer(TID tid, int4 interval_len, int *interval_ptr)
{
	gtmsource_ctl_close();
}

int gtmsource_start_jnl_release_timer(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* start_timer expects > 0 time interval in milli seconds, idle_timeout is in seconds */
	if ((TREF(replgbl)).jnl_release_timeout)
		start_timer((TID)gtmsource_jnl_release_timer, (TREF(replgbl)).jnl_release_timeout * MILLISECS_IN_SEC,
			gtmsource_jnl_release_timer, 0, NULL);
	return (SS_NORMAL);
}

int gtmsource_stop_jnl_release_timer(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(replgbl)).jnl_release_timeout)
		cancel_timer((TID)gtmsource_jnl_release_timer);
	return (SS_NORMAL);
}
