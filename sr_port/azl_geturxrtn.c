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
#include "urx.h"

GBLREF urx_rtnref urx_anchor;

bool azl_geturxrtn(char *addr, mstr *rname, urx_rtnref	**rp)
{
	assert(urx_anchor.len == 0);
	for (*rp = urx_anchor.next; *rp; *rp = (*rp)->next)
	{
		urx_addr	*ap;

		for (ap = (*rp)->addr; ap; ap = ap->next)
			if (addr == (char *)ap->addr)
				break;
		if (ap)
		{
			rname->len = (*rp)->len;
			rname->addr = (char *)&(*rp)->name[0];
			return TRUE;
		}
	}
	return FALSE;
}
