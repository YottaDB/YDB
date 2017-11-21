/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
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
#include "caller_id.h"

void cmj_err(struct CLB *lnk, cmi_reason_t reason, cmi_status_t status)
{
	struct NTD *tsk = lnk->ntd;

	ASSERT_IS_LIBCMISOCKETTCP;
	CMI_DPRINT(("CMJ_ERR called from 0x%x, reason %d, status %d\n", caller_id(), reason, status));

	lnk->deferred_event = TRUE;
	lnk->deferred_reason = reason;
	lnk->deferred_status = status;
	assertpro(FD_SETSIZE > lnk->mun);
	FD_CLR(lnk->mun, &tsk->rs);
	FD_CLR(lnk->mun, &tsk->ws);
	FD_CLR(lnk->mun, &tsk->es);
	lnk->sta = CM_CLB_DISCONNECT;
}
