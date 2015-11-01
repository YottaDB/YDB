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
#include "rtnhdr.h"
#include "urx.h"

GBLDEF urx_rtnref		urx_anchor;

void urx_resolve (rhdtyp *rtn, lent *lbl_tab, lent *lbl_top)
{
	urx_rtnref	*rp0, *rp1;
	urx_labref	*lp0, *lp1;
	urx_addr	*ap;
	void		free();

	if (!urx_getrtn (rtn->routine_name.c, mid_len (&rtn->routine_name), &rp0, &rp1, &urx_anchor))
		return;
	while ((ap = rp1->addr) != 0)
	{
		assert (*ap->addr == 0);
		*ap->addr = (int4) rtn;
		rp1->addr = ap->next;
		free (ap);
	}
	while (lbl_tab < lbl_top)
	{
		if (urx_getlab (lbl_tab->lname.c, mid_len (&lbl_tab->lname), rp1, &lp0, &lp1))
		{
			while ((ap = lp1->addr) != 0)
			{
				assert (*ap->addr == 0);
				*ap->addr = (int4) rtn + lbl_tab->laddr;
				lp1->addr = ap->next;
				free (ap);
			}
			assert (lp1->addr == 0);
			if (lp0 == (urx_labref *)rp1)
				((urx_rtnref *) lp0)->lab = lp1->next;
			else
				lp0->next = lp1->next;
			free (lp1);
		}
		lbl_tab++;
	}
	if (rp1->lab == 0)
	{
		rp0->next = rp1->next;
		free (rp1);
	}
}
