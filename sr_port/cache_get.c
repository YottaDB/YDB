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

#include "cache.h"
#include "hashtab_objcode.h"

GBLREF	int			cache_hits, cache_fails;
GBLREF	hash_table_objcode	cache_table;

/* cache_get - get cached indirect object code corresponding to input source and code from cache_table.
 *
 *	If object code exists in cache, return pointer to object code mstr
 *	otherwise, return NULL.
 */
mstr *cache_get(icode_str *indir_src)
{
	cache_entry	*csp;
	ht_ent_objcode	*tabent;

	if (NULL != (tabent = lookup_hashtab_objcode(&cache_table, indir_src)))
	{
		cache_hits++;
		return &(((cache_entry *)tabent->value)->obj);
	} else
	{
		cache_fails++;
		return NULL;
	}
}
