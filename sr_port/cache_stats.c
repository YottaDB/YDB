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

/* Print cacheing stats for indirect code */
#include "mdef.h"
#include "gtm_stdio.h"
#include "cache.h"

GBLREF int		cache_hits, cache_fails;
GBLREF cache_entry	*cache_entry_base, *cache_entry_top;

void cache_stats(void)
{
	int		total_attempts, ace;
	cache_entry	*cp;

	total_attempts = cache_hits + cache_fails;
	FPRINTF(stderr,"\nIndirect code cache performance -- Hits: %d, Fails: %d, Hit Ratio: %d%%\n",
		cache_hits, cache_fails, total_attempts ? (cache_hits * 100) / (cache_hits + cache_fails) : 0);
	ace = 0;	/* active cache entries */
	for (cp = cache_entry_base; cp < cache_entry_top; ++cp)
	{
		if (cp->refcnt)
			++ace;
	}
	FPRINTF(stderr,"Indirect cache entries currently marked active: %d\n", ace);
}

