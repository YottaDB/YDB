/****************************************************************
 *								*
 *	Copyright 2001,2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "mmemory.h"
#include "min_max.h"
#include "stringpool.h"

GBLREF mvar 	*mvartab;
GBLREF mvax 	*mvaxtab, *mvaxtab_end;
GBLREF int 	mvmax;

mvar *get_mvaddr(mident *var_name)
{
	mvar 	**p;
	mvax 	*px;
	mstr 	vname;
	int 	x;

	p = &mvartab;
	while (*p)
	{
		MIDENT_CMP(&(*p)->mvname, var_name, x);
		if (x < 0)
			p = &((*p)->rson);
		else if (x > 0)
			p = &((*p)->lson);
		else
			return *p;
	}
	/* variable doesn't exist - create a new mvar in mvartab */
	vname.len = var_name->len;
	vname.addr = var_name->addr;
	s2pool_align(&vname);
	*p = (mvar *) mcalloc((unsigned int) sizeof(mvar));
	(*p)->mvname.len = vname.len;
	(*p)->mvname.addr = vname.addr;
	(*p)->mvidx = mvmax++;
	(*p)->lson = (*p)->rson = NULL;
	(*p)->last_fetch = NULL;
	px = (mvax *) mcalloc(sizeof(mvax));
	px->var = *p;
	px->last = px->next = 0;
	px->mvidx = (*p)->mvidx;
	if (mvaxtab_end)
	{
		px->last = mvaxtab_end;
		mvaxtab_end->next = px;
		mvaxtab_end = px;
	}
	else
		mvaxtab = mvaxtab_end = px;
	return *p;
}
