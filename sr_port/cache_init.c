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
#include "mdq.h"
#include "cache.h"

GBLREF cache_entry	*cache_entry_base, *cache_entry_top, *cache_stealp, cache_temps;
GBLREF cache_tabent	*cache_tabent_base;

void cache_init(void)
{
	cache_tabent	*ctp, *ct_top;

	ctp = cache_tabent_base = (cache_tabent *)malloc(CACHE_TAB_SIZE * sizeof(cache_tabent));
	ct_top = ctp + CACHE_TAB_SIZE;
	for ( ; ctp < ct_top ; ctp++)
		ctp->fl = ctp->bl = (cache_entry *)ctp;

	/* The cache entries need several fields initialized so just clear the entire block to
	   keep do this efficiently. */
	cache_stealp = cache_entry_base = (cache_entry *)malloc((CACHE_TAB_ENTRIES) * sizeof(cache_entry));
	memset((char *)cache_stealp, 0, ((CACHE_TAB_ENTRIES) * sizeof(cache_entry)));
	cache_entry_top = cache_entry_base + CACHE_TAB_ENTRIES;
	cache_temps.linktemp.fl = cache_temps.linktemp.bl = &cache_temps;
}
