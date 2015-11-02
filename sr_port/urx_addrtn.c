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
#include <rtnhdr.h>
#include "urx.h"

urx_rtnref *urx_addrtn(urx_rtnref *rp_start, urx_rtnref *rp)
{
	urx_rtnref	*rp0, *rp1;
	unsigned char	*rtn;
	int		rtnlen, c;

	rtnlen = rp->len;
	rtn = &rp->name[0];
	assert(0 < rtnlen);
	rp0 = rp_start;
	rp1 = rp0->next;
	/* Locate if rp exists in rtn chain anchored by rp_start. If it exists, return
	   that address after putting the addr reference chain on the end of the
	   existing chain. Note that routines are ordered on this chain alphabetically
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
				return rp1;
			break;
		}
	}
	assert(rp0->next == rp1);
	rp->next = rp1;
	rp0->next = rp;
	assert(rp0->next != 0);
	return rp0->next;
}
