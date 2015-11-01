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
#include "gt_timer.h"
#include "eintr_wrappers.h"

GBLDEF struct NTD *ntd_root;

void cmi_idle(uint4 hiber)
{
	boolean_t posted = FALSE;
	sigset_t oset;
	struct CLB *lnk;
	int rc;

	/*
	 * call housekeeping twice - once to catch the
	 * async write completion, and another time
	 * to catch potential completion from an
	 * I/O interrupt.
	 */
	SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
	cmj_housekeeping();
	while (lnk = cmj_getdeferred(ntd_root))
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
		hiber_start_wait_any(hiber);
		SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
		cmj_housekeeping();
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	}
}
