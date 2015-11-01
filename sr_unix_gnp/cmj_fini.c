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

void cmj_fini(struct CLB *lnk)
{
	struct NTD *tsk = lnk->ntd;

	switch (lnk->sta)
	{
	case CM_CLB_READ:
		lnk->cbl = lnk->ios.xfer_count;
		if (tsk->trc)
			(*tsk->trc)(lnk, lnk->sta, lnk->mbf, (size_t)lnk->cbl);
		(void)cmj_clb_reset_async(lnk);
		break;
	case CM_CLB_WRITE:
		if (tsk->trc)
			(*tsk->trc)(lnk, lnk->sta, lnk->mbf, (size_t)lnk->cbl);
		(void)cmj_clb_reset_async(lnk);
		break;
	case CM_CLB_WRITE_URG:
		if (tsk->trc)
			(*tsk->trc)(lnk, lnk->sta, &lnk->urgdata, (size_t)lnk->cbl);
		(void)cmj_clb_reset_async(lnk);
		lnk->sta = lnk->prev_sta;
		return;
	case CM_CLB_IDLE:
		assert(FALSE);
		break;
	case CM_CLB_DISCONNECT:
		return;
	default:
		GTMASSERT;
	}
	lnk->sta = CM_CLB_IDLE;
	if (lnk->ast)
	{
		/* don't overrite something that is not posted */
		if (!lnk->deferred_event)
		{
			lnk->deferred_event = TRUE;
			lnk->deferred_reason = CMI_REASON_IODONE;
		}
	}
	return;
}
