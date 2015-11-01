/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

cmi_status_t cmi_write(struct CLB *lnk)
{
	sigset_t oset;
	cmi_status_t status;
	struct NTD *tsk = lnk->ntd;
	int rc;

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
		sigsuspend(&oset);
		cmj_housekeeping();
		/* recover status */
		if (lnk->sta != CM_CLB_WRITE)
			status = CMI_CLB_IOSTATUS(lnk);
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);

	CMI_DPRINT(("EXIT CMI_WRITE sta = %d\n", lnk->sta));
	return status;
}
