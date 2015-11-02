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
#include <rtnhdr.h>
#include "urx.h"

GBLREF urx_rtnref	urx_anchor;

void urx_add (urx_rtnref *lcl_anchor)
{
	urx_rtnref	*rp, *rp0;
	urx_labref	*lp, **lpp;
	urx_addr	*ap;

	/* Take a local unresolved chain describing the unresolves for a single module and merge
	   them into the global unresolved chain. If a routine or label node does not exist, the node
	   from the local chain is stolen and put into the global chain. If a node exists, the
	   necessary address references are appended to the existing list. Note that the local chain
	   is effectively destroyed by this operation. If a node was already existing and thus not
	   added into the global chain we release it to plug any memory leakage.
	 */
	assert(lcl_anchor->len == 0);
	rp0 = &urx_anchor;
	while (lcl_anchor->next)
	{	/* For each routine on the local list */
		rp = lcl_anchor->next;
		lcl_anchor->next = rp->next;
		rp0 = urx_addrtn(rp0, rp);
		if (rp0 != rp)
		{	/* The routine already existed on the global list */
			if (rp0->addr)
			{	/* Add our address refs to the end of the list */
				for (ap = rp0->addr; ap->next; ap = ap->next) ;
				ap->next = rp->addr;
			}
			lpp = &rp0->lab;
			while (rp->lab)
			{	/* For each label in the local list for this routine */
				lp = rp->lab;
				rp->lab = lp->next;
				lpp = urx_addlab(lpp, lp);
				if (lpp != &(lp->next))
					/* The label already existed */
					free(lp);
			}
			free(rp);
		}
	}
}
