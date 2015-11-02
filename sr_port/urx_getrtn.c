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
#include "gtm_string.h"
#include <rtnhdr.h> /* for urx.h */
#include "urx.h"

bool urx_getrtn (char *rtn, int rtnlen, urx_rtnref **rp0p, urx_rtnref **rp1p, urx_rtnref *anchor)
{
	boolean_t	found;
	urx_rtnref	*rp0, *rp1;
	int		c;

	assert(anchor->len == 0);
	assert(0 < rtnlen);
	found = FALSE;
	rp0 = anchor;
	rp1 = rp0->next;
	/* Locate the given routine on the unresolved routine chain. If not found we will
	   return the insertion point. Note that names are ordered on this chain alphabetically
	   within routine name size ... i.e. all sorted 1 character routines followed
	   by all sorted 2 character routines, etc.
	*/
	while (rp1 != 0)
	{
		c = rtnlen - rp1->len;
		if (!c)
			c = memcmp(rtn, &rp1->name[0], rtnlen);
		if (c > 0)
		{
			rp0 = rp1;
			rp1 = rp0->next;
		} else
		{
			if (c == 0)
				found = TRUE;
			break;
		}
	}
	assert(rp0->next == rp1);
	*rp0p = rp0;
	*rp1p = rp1;
	return found;
}
