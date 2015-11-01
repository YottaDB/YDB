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

cmi_status_t cmi_read(struct CLB *lnk)
{
	sigset_t oset;
	cmi_status_t status;
	struct NTD *tsk = lnk->ntd;
	int rc;

	CMI_DPRINT(("ENTER CMI_READ, AST 0x%x\n", lnk->ast));

	lnk->cbl = lnk->mbl;
	SIGPROCMASK(SIG_BLOCK, &tsk->mutex_set, &oset, rc);
	status = cmj_read_start(lnk);
	if (CMI_ERROR(status))
	{
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
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
		sigsuspend(&oset);
		cmj_housekeeping();
		/* recover status */
		if (lnk->sta != CM_CLB_READ)
			status = CMI_CLB_IOSTATUS(lnk);
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);

	CMI_DPRINT(("EXIT CMI_READ sta = %d\n", lnk->sta));
	return status;
}
