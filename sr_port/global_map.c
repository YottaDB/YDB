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
#include "mstrcmp.h"
#include "global_map.h"

void global_map(mstr map[],mstr *beg,mstr *end)
{
	mstr *u,*v,t1,t2,t3;
	assert(mstrcmp(beg,end) <= 0);
	for (u = map ; u->addr ; u++)
		if (mstrcmp(u,beg) >= 0)
			break;
	for (v = u ; v->addr ; v++)
		if (mstrcmp(v,end) > 0)
			break;
	if (u == v)
	{
		if ((u - map) & 1)
			return ;
		t1 = *beg;
		t2 = *end;
		while (u->addr)
		{
			t3 = *u;
			*u++ = t1;
			t1 = t3;
			t3 = *u;
			*u++ = t2;
			t2 = t3;
		}
		*u++ = t1;
		*u++ = t2;
		u->addr = 0;
		return ;
	}
	if (((u - map) & 1) == 0)
		*u++ = *beg;
	if (((v - map) & 1) == 0)
		*--v = *end;
	do
	{
		*u++ = *v;
	} while ((v++)->addr);
	return ;
}
