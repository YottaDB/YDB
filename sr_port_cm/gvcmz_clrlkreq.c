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
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "gvcmz.h"

GBLREF struct		NTD *ntd_root;
GBLREF bool		remlkreq;
GBLDEF cm_lk_response	*lk_granted, *lk_suspended;

void gvcmz_clrlkreq(void)
{
	struct CLB	*p;
	unsigned char	operation1, *ptr;
	mlk_pvtblk	*temp,*temp1;
	cm_lk_response	*q;

	if (!ntd_root || !(p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl))->usr || !remlkreq)
		return;
	operation1 = ZAREQUEST_SENT | LREQUEST_SENT | REQUEST_PENDING;
	for ( ; p != (struct CLB *)ntd_root; p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		if (((link_info*)(p->usr))->lck_info & REQUEST_PENDING)
		{
			((link_info*)(p->usr))->lck_info &= ~operation1;
			temp = ((link_info*)(p->usr))->netlocks;
			while (temp)
			{
				temp1 = temp->next;
				free(temp);
				temp = temp1;
			}
			((link_info*)(p->usr))->netlocks = 0;
			p->ast = 0;
		}
	}
	while (lk_suspended)
	{
		q = lk_suspended->next;
		lk_suspended->next = NULL;
		lk_suspended = q;
	}
	while (lk_granted)
	{
		q = lk_granted->next;
		lk_granted->next = NULL;
		lk_granted = q;
	}
	remlkreq = FALSE;
}
