/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "hashtab.h"

GBLREF hash_table_objcode	cache_table;
GBLREF	int			indir_cache_mem_size;

void cache_init(void)
{
	init_hashtab_objcode(&cache_table, ICACHE_TABLE_INIT_SIZE, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
	indir_cache_mem_size = 0;
}
