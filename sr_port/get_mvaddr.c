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

#include "compiler.h"
#include "mmemory.h"

mvar *get_mvaddr(mident *c)
{
	GBLREF mvar *mvartab;
	GBLREF mvax *mvaxtab,*mvaxtab_end;
	GBLREF int mvmax;
	char *ch;
	mvar **p;
	mvax *px;
	char *tmp;
	int x;
	ch = c->c;

	p = &mvartab;
	while (*p)
		if ((x = memcmp((*p)->mvname.c,ch,sizeof(mident) / sizeof(char)))
		    < 0)
			p = &((*p)->rson);
		else
			if (x > 0)
				p = &((*p)->lson);
			else
				return *p;
	*p = (mvar *) mcalloc((unsigned int) sizeof(mvar));
	for (tmp = (*p)->mvname.c ;
	    *ch && tmp < &((*p)->mvname.c[sizeof(mident) / sizeof(char)]) ; )
		*tmp++ = *ch++;
	while(tmp < &((*p)->mvname.c[sizeof(mident) / sizeof(char)]))
		*tmp++ = 0;
	(*p)->mvidx = mvmax++;
	(*p)->lson = (*p)->rson = 0;
	(*p)->last_fetch = 0;
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
