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
#include "gtm_string.h"
#include "rtnhdr.h"
#include "urx.h"

urx_labref **urx_addlab (urx_labref **lp0, urx_labref *lp)
{
	urx_labref	*lp1;
	int		lablen;
	unsigned char	*lab;
	urx_addr	*ap;
	int		c;
	boolean_t	found;


	found = FALSE;
	lablen = lp->len;
	lab = (unsigned char *)lp->name.c;
	lp1 = *lp0;
	/* Locate if lp exists in chain anchored by *lp0. If it is not found, the
	   local label and its attached addr chain are just added (inserted) into
	   the global chain. If it is found, we put the labels addr refs at the
	   end of the existing addr chain. Note that labels are ordered on this
	   chain alphabetically within label name size ... i.e. all sorted 1 character
	   labels followed by all sorted 2 character labels, etc.
	*/
	while (lp1 != 0)
	{	/* Find existing label or insertion point */
		c = lablen - lp1->len;
		if (!c)
			c = memcmp(lab, lp1->name.c, lablen);
		if (c > 0)
		{
			lp0 = &lp1->next;
			lp1 = *lp0;
		} else
		{
			if (c == 0)
				found = TRUE;
			break;
		}
	}
	assert(*lp0 == lp1);

	if (!found)
	{	/* Put this label in its entirety on the new chain */
		lp->next = lp1;
		*lp0 = lp;
	} else
	{
		assert (lp1->addr);
		for (ap = lp1->addr; ap->next; ap = ap->next) ; /* Find end of existing chain */
		ap->next = lp->addr; /* append new list to end */
	}
	return &(*lp0)->next;
}
