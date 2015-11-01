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

#ifdef CMI_DEBUG
#include "gtm_stdio.h"
#endif

cmi_status_t cmj_clb_set_async(struct CLB *lnk)
{
	struct NTD *tsk = lnk->ntd;
	cmi_status_t status = SS_NORMAL;

	CMI_DPRINT(("in cmj_clb_set_async sta = %d\n", lnk->sta));
	switch (lnk->sta)
	{
	case CM_CLB_READ:
		FD_SET(lnk->mun, &tsk->rs);
		break;
	case CM_CLB_WRITE:
	case CM_CLB_WRITE_URG:
		FD_SET(lnk->mun, &tsk->ws);
		break;
	default:
		assert(FALSE);
	}
	return status;
}

cmi_status_t cmj_clb_reset_async(struct CLB *lnk)
{
	struct NTD *tsk = lnk->ntd;
	cmi_status_t status = SS_NORMAL;

	CMI_DPRINT(("in cmj_clb_reset_async sta = %d\n", lnk->sta));
	switch (lnk->sta)
	{
	case CM_CLB_READ:
		FD_CLR(lnk->mun, &tsk->rs);
		break;
	case CM_CLB_WRITE:
	case CM_CLB_WRITE_URG:
		FD_CLR(lnk->mun, &tsk->ws);
		break;
	default:
		assert(FALSE);
	}
	return status;
}
