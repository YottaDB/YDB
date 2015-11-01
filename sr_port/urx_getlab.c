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

bool urx_getlab (char *lab, char lablen, urx_rtnref *rtn, urx_labref **lp0p, urx_labref **lp1p)
{
	urx_labref	*lp0, *lp1;
	urx_labref	*tmplp;
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
	if (found)
		assert (lp1 != 0);
	*lp0p = lp0;
	*lp1p = lp1;
	return found;
}
