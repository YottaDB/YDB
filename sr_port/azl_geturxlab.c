/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "urx.h"

bool azl_geturxlab (char *addr, urx_rtnref *rp)
{
	urx_labref	*lp;

	assert (rp->lab);
	for (lp = rp->lab; lp; lp = lp->next)
	{
		urx_addr	*ap;

		for (ap = lp->addr; ap; ap = ap->next)
			if (addr == (char *)ap->addr)
				break;
		if (ap)
			return TRUE;
	}
	return FALSE;
}
