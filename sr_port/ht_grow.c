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

void ht_grow(htab_desc *table)
{
	ht_entry 	*p, *q, *s;
	char 		dummy;
	unsigned int	prev_size;

	p = s = table->base;
	q = p + table->size;
	prev_size = table->size;
	/*
	Always grow according to the prime table.
	if maximum table size is reached  and it is not full, ht_init() has not effect.
	if maximum table size is reached  and it is full, ht_init() gives rts_error().
	*/
	ht_init(table, prev_size + 1);
	if (table->size > prev_size)
	{
		for ( ; p < q; p++)
		{
			if ((p->nb.val.i1 != 0) || (p->nb.val.i2 !=0))
				ht_put(table, &(p->nb), &dummy)->ptr = p->ptr;
		}
		free((char *)s);
	}
	return;
}
