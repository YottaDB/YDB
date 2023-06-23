/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <signal.h>
#include <time.h>
#include "mupip_ctrl.h"

GBLDEF bool	mu_ctrly_occurred;
GBLDEF bool	mu_ctrlc_occurred;

static int4 curr_time, prev_time = 0;

void mupip_ctrl(int sig)
{
	assert (sig == SIGINT);

	curr_time = (int4)time(0);
	if (prev_time)
	{
		if ((curr_time - prev_time) > 1)
		{
			mu_ctrlc_occurred = TRUE;
		}else
		{
			mu_ctrly_occurred = TRUE;
		}
	}else
		mu_ctrlc_occurred = TRUE;
	prev_time = curr_time;
	signal(SIGINT, mupip_ctrl);
	return;
}
