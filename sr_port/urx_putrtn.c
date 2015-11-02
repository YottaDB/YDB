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
#include <stddef.h>
#include "gtm_string.h"
#include <rtnhdr.h> /* needed by urx.h */
#include "urx.h"

urx_rtnref *urx_putrtn (char *rtn, int rtnlen, urx_rtnref *anchor)
{
	urx_rtnref	*rp0, *rp1, *tmp;
	boolean_t	found;
	int		c;

	assert(anchor->len == 0);
	assert(0 < rtnlen);
	found = FALSE;
	rp0 = anchor;
	rp1 = rp0->next;
	/* Looks for given routine name on routine unresolved list. If it is not found,
	   a new node is created and added to the chain. The address of the found or
	   created node is returned. Note that routines are ordered on this chain alphabetically
	   within routine name size ... i.e. all sorted 1 character routines followed
	   by all sorted 2 character routines, etc.
	*/
	while (rp1 != 0)
	{	/* Find routine or insertion point */
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
	if (!found)
	{
		tmp = (urx_rtnref *)malloc(offsetof(urx_rtnref, name[0]) + rtnlen);
		tmp->len = rtnlen;
		memcpy(&tmp->name[0], rtn, rtnlen);
		tmp->addr = NULL;
		tmp->lab = NULL;
		tmp->next = rp1;
		rp0->next = tmp;
	}
	assert(rp0->next != NULL);
	return rp0->next;
}
