/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
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
#include "cmidef.h"
#include "eintr_wrappers.h"

#define CMI_WRITE_URG_EPOLL_MAX_EVENTS	16

cmi_status_t cmi_write_urg(struct CLB *lnk, unsigned char urg)
{
	cmi_status_t status;
	struct NTD *tsk = lnk->ntd;
	struct epoll_event evs[CMI_WRITE_URG_EPOLL_MAX_EVENTS];
	sigset_t oset;
	int rc;

	ASSERT_IS_LIBCMISOCKETTCP;
	lnk->urgdata = urg;
	SIGPROCMASK(SIG_BLOCK, &tsk->mutex_set, &oset, rc);
	status = cmj_write_urg_start(lnk);
	if (CMI_ERROR(status))
	{
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
		return status;
	}
	cmj_housekeeping();
	while (lnk->sta == CM_CLB_WRITE_URG && !lnk->ast)
	{
		/* Wait for any registered fd to become ready (was sigsuspend on SIGIO). */
		(void)epoll_wait(tsk->epoll_fd, evs, CMI_WRITE_URG_EPOLL_MAX_EVENTS, -1);
		tsk->sigio_interrupt = TRUE;
		cmj_housekeeping();
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	return status;
}
