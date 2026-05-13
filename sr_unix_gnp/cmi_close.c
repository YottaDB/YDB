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
#include "cmidef.h"
#include "relqop.h"
#include "eintr_wrappers.h"
#include "gtmio.h"

error_def(ERR_FDSIZELMT);

cmi_status_t cmi_close(struct CLB *lnk)
{
	cmi_status_t  status = SS_NORMAL;
	que_ent_ptr_t qp;
	struct CLB *previous;
	sigset_t oset;
	struct NTD *tsk = lnk->ntd;
	int rc;

	ASSERT_IS_LIBCMISOCKETTCP;
	qp = RELQUE2PTR(lnk->cqe.fl);
	previous = QUEENT2CLB(qp, cqe);
	remqt(&previous->cqe);
	if (FD_SETSIZE <= lnk->mun)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_FDSIZELMT, 1, lnk->mun);
	cmj_epoll_remove(tsk, lnk->mun);
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
	cmj_housekeeping();
	return status;
}
