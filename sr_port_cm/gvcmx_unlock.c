/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "gvcmx.h"
#include "gvcmz.h"

GBLREF spdesc	stringpool;
GBLREF struct	NTD *ntd_root;
GBLREF bool	remlkreq;

void gvcmx_unlock(unsigned char rmv_locks, bool specific, char incr)
{
	struct CLB	*p;
	unsigned char	operation1,operation2,*ptr;

	if (!ntd_root || !ntd_root->cqh.fl || (specific && !remlkreq))
		return;

	if (rmv_locks == CM_ZALLOCATES)
	{
		operation1 = CM_ZALLOCATES;
		operation2 = REMOTE_ZALLOCATES;
	} else
	{
		operation1 = CM_LOCKS;
		operation2 = REMOTE_LOCKS;
	}
	operation1 |= incr;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); p != (struct CLB *)ntd_root; p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		p->ast = 0;
		ENSURE_STP_FREE_SPACE(p->mbl);
		p->mbf = stringpool.free;
		if (remlkreq)
		{
			if (((link_info*)(p->usr))->lck_info & REQUEST_PENDING)
				gvcmz_sndlkremove(p, operation1, CMMS_L_LKDELETE);
		} else if (((link_info*)(p->usr))->lck_info & (REMOTE_LOCKS | REMOTE_ZALLOCATES))
		{
			gvcmz_sndlkremove(p, operation1, CMMS_L_LKCANALL);
			((link_info*)(p->usr))->lck_info &= ~operation2;
		}
	}
}
