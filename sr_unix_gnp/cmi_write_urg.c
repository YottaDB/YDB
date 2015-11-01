/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "eintr_wrappers.h"

cmi_status_t cmi_write_urg(struct CLB *lnk, unsigned char urg)
{
	cmi_status_t status;
	struct NTD *tsk = lnk->ntd;
	sigset_t oset;
	int rc;

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
		sigsuspend(&oset);
		cmj_housekeeping();
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	return status;
}
