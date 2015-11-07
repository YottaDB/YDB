/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <ssdef.h>
#include <descrip.h>

#include "mdef.h"
#include "comp_esc.h"

GBLREF struct ce_sentinel_desc	*ce_def_list;


int4 GTM$CE_ESTABLISH (user_action_routine, sentinel)
int4			(*user_action_routine)();
struct dsc$descriptor_s	sentinel;
{
	struct ce_sentinel_desc	*shp, *shp_last;


	shp = shp_last = ce_def_list;
	while (shp != NULL)
	{
		shp_last = shp;
		shp = shp->next;
	}

	/* Create new node. */
	shp = malloc(SIZEOF(struct ce_sentinel_desc));

	shp->escape_sentinel = malloc(sentinel.dsc$w_length);
	memcpy (shp->escape_sentinel, sentinel.dsc$a_pointer, sentinel.dsc$w_length);
	shp->escape_length = sentinel.dsc$w_length;
	shp->user_routine = user_action_routine;
	shp->next = NULL;

	/* Add to list. */
	if (shp_last == NULL)
	{
		/* First sentinel handler, it is the list. */
		ce_def_list = shp;
	}
	else
	{
		/* Add to end of list. */
		shp_last->next = shp;
	}


	return SS$_NORMAL;
}
