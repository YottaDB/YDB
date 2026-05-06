/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include <sys/epoll.h>
#include <errno.h>
#include "cmidef.h"
#include "gt_timer.h"
#include "eintr_wrappers.h"

#define CMI_EPOLL_MAX_EVENTS	64

GBLREF struct NTD *ntd_root;

void cmi_idle(uint4 hiber)
{
	boolean_t posted = FALSE;
	sigset_t oset;
	struct CLB *lnk;
	struct epoll_event evs[CMI_EPOLL_MAX_EVENTS];
	int rc, nev;

	ASSERT_IS_LIBCMISOCKETTCP;
	/*
	 * call housekeeping twice - once to catch the
	 * async write completion, and another time
	 * to catch potential completion from an
	 * I/O interrupt.
	 */
	SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
	cmj_housekeeping();
	while ((lnk = cmj_getdeferred(ntd_root)))
	{
		lnk->deferred_event = FALSE;
		posted = TRUE;
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
		cmj_postevent(lnk);
		SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	if (!posted)
	{
		/* Wait for any registered fd to become ready, or for the hiber timeout to expire. We don't need
		 * to inspect the returned events: the existing cmj_select() machinery (driven by sigio_interrupt)
		 * dispatches based on rs/ws/es, which we keep in sync with the epoll set. Returning on EINTR
		 * matches the prior hiber_start_wait_any() semantics so that timer / shutdown signals get prompt
		 * housekeeping.
		 */
		nev = epoll_wait(ntd_root->epoll_fd, evs, CMI_EPOLL_MAX_EVENTS, (int)hiber);
		if (0 < nev)
			ntd_root->sigio_interrupt = TRUE;
		SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
		cmj_housekeeping();
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	}
}
