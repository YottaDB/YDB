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

urx_labref **urx_addlab (urx_labref **anchor, urx_labref *lp)
{
	urx_labref	*target;
	int		lablen;
	unsigned char	*lab;
	urx_addr	*ap;
	int		c;
	boolean_t	found;


	found = FALSE;
	lablen = lp->len;
	lab = &lp->name[0];
	target = *anchor;
	/* Locate if lp exists in chain anchored by *anchor. If it is not found, the
	   local label and its attached addr chain are just added (inserted) into
	   the global chain. If it is found, we put the labels addr refs at the
	   end of the existing addr chain. Note that labels are ordered on this
	   chain alphabetically within label name size ... i.e. all sorted 1 character
	   labels followed by all sorted 2 character labels, etc.
	*/
	while (target != 0)
	{	/* Find existing label or insertion point */
		c = lablen - target->len;
		if (!c)
			c = memcmp(lab, &target->name[0], lablen);
		if (c > 0)
		{
			anchor = &target->next;
			target = *anchor;
		} else
		{
			if (c == 0)
				found = TRUE;
			break;
		}
	}
	assert(*anchor == target);

	if (!found)
	{	/* Put this label in its entirety on the new chain */
		lp->next = target;
		*anchor = lp;
	} else
	{
		assert (target->addr);
		for (ap = target->addr; ap->next; ap = ap->next) ; /* Find end of existing chain */
		ap->next = lp->addr; /* append new list to end */
	}
	return &(*anchor)->next;
}
