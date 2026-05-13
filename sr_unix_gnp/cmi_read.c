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

#define CMI_READ_EPOLL_MAX_EVENTS	16

cmi_status_t cmi_read(struct CLB *lnk)
{
	sigset_t oset;
	cmi_status_t status;
	struct NTD *tsk = lnk->ntd;
	struct epoll_event evs[CMI_READ_EPOLL_MAX_EVENTS];
	int rc;

	ASSERT_IS_LIBCMISOCKETTCP;
	CMI_DPRINT(("ENTER CMI_READ, AST 0x%x\n", lnk->ast));

	lnk->cbl = lnk->mbl;
	status = cmj_read_start(lnk);
	if (CMI_ERROR(status))
	{
		CMI_DPRINT(("EXIT CMI_READ ERROR CODE %d\n", status));
		return status;
	}
	/*
	 * At this point see if the I/O has completed
	 * by probing the sta CLB field.
	 */
	cmj_housekeeping();
	while (lnk->sta == CM_CLB_READ && !lnk->ast)
	{
		/* Wait for any registered fd to become ready (was sigsuspend on SIGIO). */
		(void)epoll_wait(tsk->epoll_fd, evs, CMI_READ_EPOLL_MAX_EVENTS, -1);
		tsk->sigio_interrupt = TRUE;
		cmj_housekeeping();
		/* recover status */
		if (lnk->sta != CM_CLB_READ)
			status = CMI_CLB_IOSTATUS(lnk);
	}

	CMI_DPRINT(("EXIT CMI_READ sta = %d\n", lnk->sta));
	return status;
}
