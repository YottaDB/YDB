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
#include "urx.h"

GBLREF urx_rtnref		urx_anchor;

void urx_add (urx_rtnref *lcl_anchor)
{
	urx_rtnref	*rp, *rp0;
	urx_labref	*lp, **lpp;
	urx_addr	*ap;

	assert (lcl_anchor->len == 0);
	rp0 = &urx_anchor;
	while (lcl_anchor->next)
	{
		rp = lcl_anchor->next;
		lcl_anchor->next = rp->next;
		rp0 = urx_addrtn (rp0, rp);
		if (rp0 != rp)
		{
			if (rp0->addr)
			{
				for (ap = rp0->addr; ap->next; ap = ap->next) ;
				ap->next = rp->addr;
			}
			lpp = &rp0->lab;
			while (rp->lab)
			{
				lp = rp->lab;
				rp->lab = lp->next;
				lpp = urx_addlab (lpp, lp);
			}
		}
	}
}
