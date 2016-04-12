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

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "io.h"
#include "min_max.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "cachectl.h"
#include "gtm_text_alloc.h"
#include "error.h"

GBLREF	hash_table_objcode	cache_table;
GBLREF	int			indir_cache_mem_size;

error_def(ERR_MEMORY);
error_def(ERR_VMSMEMORY);

void cache_table_rebuild()
{
	ht_ent_objcode 	*tabent, *topent;
	cache_entry	*csp;

	DBGCACHE((stdout, "cache_table_rebuild: Rebuilding indirect lookaside cache\n"));
	for (tabent = cache_table.base, topent = cache_table.top; tabent < topent; tabent++)
	{
		if (HTENT_VALID_OBJCODE(tabent, cache_entry, csp))
		{
			if ((0 == csp->refcnt) && (0 == csp->zb_refcnt))
			{
				((ihdtyp *)(csp->obj.addr))->indce = NULL;
				indir_cache_mem_size -= (ICACHE_SIZE + csp->obj.len);
				GTM_TEXT_FREE(csp);
				delete_hashtab_ent_objcode(&cache_table, tabent);
			}
		}
	}
	/* Only do compaction processing if we are not processing a memory type error (which
	 * involves allocating a smaller table with storage we don't have.
	 */
	if (COMPACT_NEEDED(&cache_table) && error_condition != UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY))
		compact_hashtab_objcode(&cache_table);
}
