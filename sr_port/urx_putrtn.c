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

urx_rtnref *urx_putrtn (char *rtn, char rtnlen, urx_rtnref *anchor)
{
	urx_rtnref	*rp0, *rp1, *tmp;
	bool		found;
	int		c;

	assert (anchor->len == 0);
	assert (0 < rtnlen);
	found = FALSE;
	rp0 = anchor;
	rp1 = rp0->next;
	while (rp1 != 0)
	{
		c = rtnlen - rp1->len;
		if (!c) c = memcmp (rtn, rp1->name.c, rtnlen);
		if (c > 0)
		{
			rp0 = rp1;
			rp1 = rp0->next;
		}
		else
		{
			if (c == 0)	found = TRUE;
			break;
		}
	}
	assert (rp0->next == rp1);
	if (!found)
	{
		tmp = (urx_rtnref *)malloc (sizeof (urx_rtnref));
		tmp->len = rtnlen;
		memcpy (tmp->name.c, rtn, rtnlen);
		tmp->addr = 0;
		tmp->lab = 0;
		tmp->next = rp1;
		rp0->next = tmp;
	}
	assert (rp0->next != 0);
	return rp0->next;
}
