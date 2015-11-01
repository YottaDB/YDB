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
#include "hashdef.h"

ht_entry *ht_get(table,target)
htab_desc *table;
mname *target;
{
ht_entry *p;
uint4 *ptr, *tar;

p = table->base + hash(target,table->size);
for ( ; p; p = p->link)
{
	tar = (uint4*)target->txt;
	ptr = (uint4*)p->nb.txt;
	if (*ptr++ != *tar++ || *ptr != *tar)
		continue;
	return p;
}
return 0;
}
