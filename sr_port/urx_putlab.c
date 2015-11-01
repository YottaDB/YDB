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

void urx_putlab (lab, lablen, rtn, addr)
char		*lab, lablen;
urx_rtnref	*rtn;
char		*addr;
{
	urx_labref	*lp0, *lp1, *tmplp;
	bool		found;
	int		c;
	urx_addr	*tmpap;

	found = FALSE;
	lp0 = (urx_labref *)rtn;
	lp1 = ((urx_rtnref *) lp0)->lab;
	while (lp1 != 0)
	{
		c = lablen - lp1->len;
		if (!c) c = memcmp (lab, lp1->name.c, lablen);
		if (c > 0)
		{
			lp0 = lp1;
			lp1 = lp0->next;
		}
		else
		{
			if (c == 0)	found = TRUE;
			break;
		}
	}
	if (lp0 == (urx_labref *)rtn)
		assert (((urx_rtnref *) lp0)->lab == lp1);
	else
		assert (lp0->next == lp1);

	if (!found)
	{
		tmplp = (urx_labref *)malloc (sizeof (urx_labref));
		tmplp->len = lablen;
		memcpy (tmplp->name.c, lab, lablen);
		tmplp->addr = 0;
		tmplp->next = lp1;
		if (lp0 == (urx_labref *)rtn)
		{
			((urx_rtnref *) lp0)->lab = tmplp;
		}
		else
		{
			lp0->next = tmplp;
		}
		lp1 = tmplp;
	}
	assert (lp1 != 0);

	tmpap = (urx_addr *)malloc (sizeof (urx_addr));
	tmpap->next = lp1->addr;
	tmpap->addr = (int4 *)addr;
	lp1->addr = tmpap;
	return;
}
