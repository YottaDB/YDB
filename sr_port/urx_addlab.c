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

#include "gtm_string.h"

#include "urx.h"

urx_labref **urx_addlab (urx_labref **lp0, urx_labref *lp)
{
	urx_labref	*lp1;
	short		lablen;
	unsigned char	*lab;
	urx_addr	*ap;
	int		c;
	bool		found;


	found = FALSE;
	lablen = lp->len;
	lab = (unsigned char *)lp->name.c;
	lp1 = *lp0;
	while (lp1 != 0)
	{
		c = lablen - lp1->len;
		if (!c) c = memcmp (lab, lp1->name.c, lablen);
		if (c > 0)
		{
			lp0 = &lp1->next;
			lp1 = *lp0;
		}
		else
		{
			if (c == 0) found = TRUE;
			break;
		}
	}
	assert (*lp0 == lp1);

	if (!found)
	{
		lp->next = lp1;
		*lp0 = lp;
	}
	else
	{
		assert (lp1->addr);
		for (ap = lp1->addr; ap->next; ap = ap->next) ;
		ap->next = lp->addr;
	}
	return &(*lp0)->next;
}
