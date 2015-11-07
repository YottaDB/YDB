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

#include "cmihdr.h"
#include "cmidef.h"

void cmj_fini(lnk)
struct CLB *lnk;
{
	switch (lnk->sta)
	{
	case CM_CLB_READ:
		lnk->cbl = lnk->ios.xfer_count;
		break;
	case CM_CLB_WRITE:
		break;
	case CM_CLB_IDLE:
		assert(FALSE);
		break;
	case CM_CLB_DISCONNECT:
		return;
	}
	lnk->sta = CM_CLB_IDLE;
	return;
}
