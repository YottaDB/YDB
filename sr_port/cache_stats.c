/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Print cacheing stats for indirect code */
#include "mdef.h"

#include "gtm_stdio.h"
#include "cache.h"
#include "hashtab_objcode.h"

GBLREF int			cache_hits, cache_fails;
GBLREF	hash_table_objcode	cache_table;

void cache_stats(void)
{
	int		total_attempts, ace;
	ht_ent_objcode 	*tabent, *topent;
	cache_entry	*csp;

	total_attempts = cache_hits + cache_fails;
	FPRINTF(stderr,"\nIndirect code cache performance -- Hits: %d, Fails: %d, Hit Ratio: %d%%\n",
		cache_hits, cache_fails, total_attempts ? ((100 * cache_hits) / (cache_hits + cache_fails)) : 0);
	ace = 0;	/* active cache entries */
	for (tabent = cache_table.base, topent = cache_table.top; tabent < topent; tabent++)
	{
		if (HTENT_VALID_OBJCODE(tabent, cache_entry, csp))
		{
			if (csp->refcnt || csp->zb_refcnt)
				++ace;
		}
	}
	FPRINTF(stderr,"Indirect cache entries currently marked active: %d\n", ace);
}

