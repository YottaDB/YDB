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

bool urx_getlab (char *lab, int lablen, urx_rtnref *rtn, urx_labref **lp0p, urx_labref **lp1p)
{
	urx_labref	*lp0, *lp1;
	urx_labref	*tmplp;
	boolean_t	found;
	int		c;
	urx_addr	*tmpap;

	found = FALSE;
	lp0 = (urx_labref *)rtn;
	lp1 = rtn->lab;
	/* Locate the given label on this routine's unresolved label chain. If not found we
	   will return the insertion point. Note that labels are ordered on this chain alphabetically
	   within label name size ... i.e. all sorted 1 character labels followed
	   by all sorted 2 character labels, etc. Note also that the layouts of urx_rtnref and urx_labref
	   are critical as the "next" field in labref is at the same offset as the "lab" anchor in the
	   rtnref block allowing urx_rtnref to be cast and serve as an anchor for the labref chain.
	*/
	while (lp1 != 0)
	{
		c = lablen - lp1->len;
		if (!c)
			c = memcmp(lab, &lp1->name[0], lablen);
		if (c > 0)
		{
			lp0 = lp1;
			lp1 = lp0->next;
		} else
		{
			if (c == 0)
				found = TRUE;
			break;
		}
	}
	if (lp0 == (urx_labref *)rtn)
		assert(((urx_rtnref *)lp0)->lab == lp1);
	else
		assert(lp0->next == lp1);
	if (found)
		assert(lp1 != 0);
	*lp0p = lp0;
	*lp1p = lp1;
	return found;
}
