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
#include "relqop.h"
#include "eintr_wrappers.h"
#include "gtmio.h"

cmi_status_t cmi_close(struct CLB *lnk)
{
	cmi_status_t  status = SS_NORMAL;
	que_ent_ptr_t qp;
	struct CLB *previous;
	sigset_t oset;
	struct NTD *tsk = lnk->ntd;
	int rc;

	qp = RELQUE2PTR(lnk->cqe.fl);
	previous = QUEENT2CLB(qp, cqe);
	remqt(&previous->cqe);
	CLOSEFILE(lnk->mun, status);
	FD_CLR(lnk->mun, &tsk->rs);
	FD_CLR(lnk->mun, &tsk->ws);
	FD_CLR(lnk->mun, &tsk->es);
	lnk->mun = -1;
	lnk->sta = CM_CLB_DISCONNECT;
	if (tsk->trc)
		(*tsk->trc)(lnk, lnk->sta, (unsigned char *)"", 0);
#ifdef notdef
	/*
	 * I really think this free should be a client
	 * reponsibility, since he allocated it.
	 *
	 */
	cmi_free_clb(lnk);
#endif
	SIGPROCMASK(SIG_BLOCK, &tsk->mutex_set, &oset, rc);
	cmj_housekeeping();
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	return status;
}
