/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"
#include "mvalconv.h"
#include "getjobnum.h"
#include "getjobname.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#include "gt_timer.h"
#endif

GBLREF	uint4	process_id;
GBLDEF	mval	dollar_job;

static	char	djbuff[10];	/* storage for dollar job's string form */

void getjobname(void)
{
	getjobnum();
	i2usmval(&dollar_job, process_id);
	n2s(&dollar_job);
	assert(dollar_job.str.len <= SIZEOF(djbuff));
	memcpy(djbuff,dollar_job.str.addr,dollar_job.str.len);
	dollar_job.str.addr = djbuff;
#	ifdef DEBUG
	/* The below white-box code was previously in INVOKE_INIT_SECSHR_ADDRS but when it was removed, the white-box
	 * code was moved over to "getjobname" which was (and continues to be) invoked just before when INVOKE_INIT_SECSHR_ADDRS
	 * used to be invoked.
	 */
	if (WBTEST_ENABLED(WBTEST_SLAM_SECSHR_ADDRS))
	{	/* For this white box test, we're going to send ourselves a SIGTERM termination signal at a specific point
		 * in the processing to make sure it succeeds without exploding during database initialization. To test the
		 * condition GTM-8455 fixes.
		 */
		kill(process_id, SIGTERM);
		hiber_start(20 * 1000);			/* Wait up to 20 secs - don't use wait_any as the heartbeat timer
							 * will kill this wait in 0-7 seconds or so.
							 */
		/* We sent, we waited, wait expired - weird - funky condition is for identification purposes (to identify the
		 * actual assert). We should be dead or dying, not trying to resume.
		 */
		assert(WBTEST_SLAM_SECSHR_ADDRS == 0);
	}
#	endif

}
