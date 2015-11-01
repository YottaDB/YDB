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

urx_rtnref *urx_addrtn (urx_rtnref *rp_start, urx_rtnref *rp)
{
	urx_rtnref	*rp0, *rp1;
	char		*rtn;
	short		rtnlen;
	bool		found;
	int		c;

	rtnlen = rp->len;
	rtn = rp->name.c;
	assert (0 < rtnlen);
	found = FALSE;
	rp0 = rp_start;
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
			if (c == 0) return rp1;
			break;
		}
	}
	assert (rp0->next == rp1);
	rp->next = rp1;
	rp0->next = rp;
	assert (rp0->next != 0);
	return rp0->next;
}
