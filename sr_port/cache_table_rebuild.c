/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "min_max.h"
#include "cache.h"
#include "hashtab.h"
#include "hashtab_objcode.h"
#include "cachectl.h"

GBLREF	hash_table_objcode	cache_table;
GBLREF	int			indir_cache_mem_size;

void cache_table_rebuild()
{
	ht_ent_objcode 	*tabent, *topent;
	cache_entry	*csp;
	for (tabent = cache_table.base, topent = cache_table.top; tabent < topent; tabent++)
	{
		if (HTENT_VALID_OBJCODE(tabent, cache_entry, csp))
		{
			if (0 == csp->refcnt && 0 == csp->zb_refcnt)
			{
				((ihdtyp *)(csp->obj.addr))->indce = NULL;
				indir_cache_mem_size -= (ICACHE_SIZE + csp->obj.len);
				free(csp);
				DELETE_HTENT((&cache_table), tabent);
			}
		}
	}
	if (COMPACT_NEEDED(&cache_table))
		compact_hashtab_objcode(&cache_table);
}
