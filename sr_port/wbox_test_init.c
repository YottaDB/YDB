/****************************************************************
 *								*
 * Copyright (c) 2005-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "iosp.h"
#include "ydb_trans_log_name.h"
#include "wbox_test_init.h"
#if defined (DEBUG) && !defined (STATIC_ANALYSIS)
#include "gtm_pthread.h"
#include "sleep.h"
#endif

void wbox_test_init(void)
{
#	if defined (DEBUG) && !defined (STATIC_ANALYSIS)
	mstr	trans_name;
	char	trans_bufr[MAX_TRANS_NAME_LEN];

	if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_ENABLE, &trans_name,
							trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
	{
		ydb_white_box_test_case_enabled = TRUE;
		if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_NUMBER, &trans_name,
							trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
		{
			ydb_white_box_test_case_number = ATOI(trans_name.addr);
			if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_COUNT, &trans_name,
								trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
				ydb_white_box_test_case_count = ATOI(trans_name.addr);
		}
	}
#	endif
}

#if defined (DEBUG) && !defined (STATIC_ANALYSIS)
static pthread_mutex_t	wbox_fork_lock = PTHREAD_MUTEX_INITIALIZER;

static void wbox_fork_lock_acquire(void)
{
	pthread_mutex_lock(&wbox_fork_lock);
}

static void wbox_fork_lock_release(void)
{
	pthread_mutex_unlock(&wbox_fork_lock);
}
#endif

/* Called with interrupts deferred, just before a malloc() of "size" bytes, when WBTEST_HOLD_FORK_LOCK_IN_MALLOC is enabled.
 * It stands in for an allocator (such as ASAN's) whose pthread_atfork() prepare handler waits for a lock that its malloc()
 * holds: it takes a lock that its own prepare handler also takes, and holds it for WBTEST_HOLD_FORK_LOCK_SECONDS. A fork()
 * by this thread in that window (from a signal handler) never returns. It acts once, on the first large enough malloc().
 */
void wbox_hold_fork_lock_in_malloc(size_t size)
{
#	if defined (DEBUG) && !defined (STATIC_ANALYSIS)
	static boolean_t	done;

	if (done || (WBTEST_HOLD_FORK_LOCK_MIN_SIZE > size))
		return;
	done = TRUE;
	pthread_atfork(wbox_fork_lock_acquire, wbox_fork_lock_release, wbox_fork_lock_release);
	wbox_fork_lock_acquire();
	FPRINTF(stderr, "WBTEST_HOLD_FORK_LOCK_IN_MALLOC : holding the fork lock in malloc()\n");
	fflush(stderr);
	/* Sleep without handling deferred signals, so that, as with a real malloc(), they are handled only after the release */
	CLOCK_NANOSLEEP(CLOCK_MONOTONIC, WBTEST_HOLD_FORK_LOCK_SECONDS, 0, RESTART_TRUE, MT_SAFE_FALSE);
	wbox_fork_lock_release();
#	endif
}
