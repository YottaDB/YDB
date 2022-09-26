/****************************************************************
 *								*
 * Copyright (c) 2008-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"

#include "gdsfhead.h"

#include <sys/types.h>
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"

/* This function is called only from White Box testing, so getting it inline or macro-ised is not a performance concern
 * It is called with the wait state being sent into UPDATE_PROC_WAIT_STATE() and if it is the one under test,
 * will coordinate a pause in execution with the test framework using a lock file.  In general it will leave the querying
 * of the statistics to the test script, but there are a few special cases where it all must be done here.
 * Note that we abuse ydb_white_box_test_case_count a bit because it is an environment variable we get for "free" and
 * is not otherwise used in this testing.
 *
 * csa_generic: Csa pointer
 * ws: The state being entered
 * incval: Increment or decrement value for the state counter
 */
void wb_gtm8863_lock_pause(void *csa_generic, wait_state ws, int incval)
{
	static int never_fired = 1;	/* only fires once per run */
	struct stat statbuf;
	char buf[80];
	int fd;
	sgmnt_addrs *csa = (sgmnt_addrs *) csa_generic;

	/* only fire for sets (incval > 0) */
	if (0 > incval)
		return;

	/* This special case for WS_47 is unfortunate, as we hoped to leverage the automatic handling
	 * of existing WB env vars in all cases, but that codepath fires so often that it fires in our driver M script
	 * before we are ready to come here */
	if ( (WS_47 == ws) && (ws == ydb_white_box_test_case_count) && (! getenv("gtm8863_wbox_ws47")))
		return;

	/* WS_41 (in PRC) is also unfortunate as it happens during rundown and shared stats are locked out
	   (it may not even be directly observable), so we manually dump the PRC counter here as better than naught */
	if ( (WS_41 == ws) && (ws == ydb_white_box_test_case_count) )
	{
		fd = creat("prc.txt", 0666);
		snprintf(buf, SIZEOF(buf), "%llu", (long long unsigned int)csa->gvstats_rec_p->f_proc_wait);
		write(fd, STR_AND_LEN(buf));
		close(fd);
	}

	/* As is WS_82 (in ZAD)*/
	if ( (WS_82 == ws) && (ws == ydb_white_box_test_case_count) )
	{
		fd = creat("zad.txt", 0666);
		snprintf(buf, SIZEOF(buf), "%llu", (long long unsigned int)csa->gvstats_rec_p->f_util_wait);
		write(fd, STR_AND_LEN(buf));
		close(fd);
	}

	if( (ws == ydb_white_box_test_case_count) && never_fired)
	{
		close(creat("gtm8863.lck", 0666));
		while(0 == stat("gtm8863.lck", &statbuf))
		{
			LONG_SLEEP(1);
		}
		never_fired = 0;
	}
}
