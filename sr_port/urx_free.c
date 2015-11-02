/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h> /* for urx.h */
#include "urx.h"

void urx_free (urx_rtnref *anchor)
{
	urx_rtnref	*rp;
	urx_labref	*lp;
	urx_addr	*ap;

	/* Release entire given (local) unresolved chain (done on error) */
	assert(anchor->len == 0);
	while (anchor->next != 0)
	{
		rp = anchor->next;
		anchor->next = rp->next;
		while (rp->addr != 0)
		{	/* release address chain for this routine */
			ap = rp->addr;
			rp->addr = ap->next;
			free(ap);
		}
		while (rp->lab != 0)
		{
			lp = rp->lab;
			while (lp->addr != 0)
			{	/* release address chain for this label */
				ap = lp->addr;
				lp->addr = ap->next;
				free(ap);
			}
			rp->lab = lp->next;
			free(lp);	/* release label node */
		}
		free(rp);	/* release routine node */
	}
}
