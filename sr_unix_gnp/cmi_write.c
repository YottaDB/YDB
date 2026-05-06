/****************************************************************
 *								*
 * Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

#define CMI_WRITE_EPOLL_MAX_EVENTS	16

cmi_status_t cmi_write(struct CLB *lnk)
{
	sigset_t oset;
	cmi_status_t status;
	struct NTD *tsk = lnk->ntd;
	struct epoll_event evs[CMI_WRITE_EPOLL_MAX_EVENTS];
	int rc;

	ASSERT_IS_LIBCMISOCKETTCP;
	CMI_DPRINT(("ENTER CMI_WRITE, AST 0x%x\n", lnk->ast));

	SIGPROCMASK(SIG_BLOCK, &tsk->mutex_set, &oset, rc);
	status = cmj_write_start(lnk);
	if (CMI_ERROR(status))
	{
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
		CMI_DPRINT(("EXIT CMI_WRITE ERROR %d\n", status));
		return status;
	}
	cmj_housekeeping();
	while (lnk->sta == CM_CLB_WRITE && !lnk->ast)
	{
		/* Wait for any registered fd to become ready (was sigsuspend on SIGIO). */
		(void)epoll_wait(tsk->epoll_fd, evs, CMI_WRITE_EPOLL_MAX_EVENTS, -1);
		tsk->sigio_interrupt = TRUE;
		cmj_housekeeping();
		/* recover status */
		if (lnk->sta != CM_CLB_WRITE)
			status = CMI_CLB_IOSTATUS(lnk);
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);

	CMI_DPRINT(("EXIT CMI_WRITE sta = %d\n", lnk->sta));
	return status;
}
