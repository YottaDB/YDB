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

#ifdef HT_TRACE
#include "gtm_stdio.h"
#endif

ht_entry *ht_put(htab_desc *table, mname *target, char *stash)
{

	/* stash set to 0 if already present, else 1 */

	ht_entry 	*p;
	int4 		*pt0, *pt1, *pt2;
	unsigned int	prev_size;

	pt0 = pt1 = (int4 *)target;
	p = (ht_entry *)(table->base + hash(target, table->size));
	pt2 = (int4 *)p;
	if (*pt1++ == *pt2++ && *pt1 == *pt2)
		*stash = 0;
	else if ((p->nb.val.i1 == 0) && (p->nb.val.i2 ==0))
	{
		if (++table->count >= table->trigger_size)
		{
			prev_size = table->size;
			ht_grow(table);
#ifdef HT_TRACE
			FPRINTF(stderr, "\nHT TRACE: Hash table %lx grew from %d to %d elements\n", (caddr_t)table,
				prev_size, table->size);
			fflush(stderr);
#endif
			if (prev_size != table->size)
				return(ht_put(table, target, stash));
		}
		*stash = 1;
		p->nb = *target;
	} else
	{
		for (;;)
		{
			pt1 = pt0;
			pt2 = (int4 *)p;
			if (*pt1++ == *pt2++ && *pt1 == *pt2)
			{
				*stash = 0;
				break;
			} else if (p->link)
			{
				p = p->link;
			} else
			{
				for (;;)
				{
					--(table->r);
					assert(table->r >= table->base);
					if ((table->r->nb.val.i1 == 0) && (table->r->nb.val.i2 ==0))
					{
						if (++table->count >= table->trigger_size)
						{
							prev_size = table->size;
							ht_grow(table);
#ifdef HT_TRACE
							FPRINTF(stderr,
								"HT TRACE: Hash table %lx grew from %d to %d elements\n",
								(caddr_t)table, prev_size, table->size);
							fflush(stderr);
#endif
							if (prev_size != table->size)
								return(ht_put(table, target, stash));
						}
						p->link = table->r;
						p = p->link;
						p->nb = *target;
						*stash = 1;
						break;
					}
				}
				break;
			}
		}
	}
	return(p);
}
