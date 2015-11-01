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
#include "cache.h"
#include "hashtab_objcode.h"
#include "cachectl.h"
#include "cacheflush.h"
#include "rtnhdr.h"

GBLREF	hash_table_objcode	cache_table;
GBLREF	int			indir_cache_mem_size;

void cache_put(icode_str *src, mstr *object)
{
	cache_entry	*csp;
	int		i, fixup_cnt;
	mval		*fix_base, *fix;
	var_tabent	*var_base, *varent;
	ht_ent_objcode	*tabent;
	boolean_t	added;

	indir_cache_mem_size += (ICACHE_SIZE + object->len);
	if (indir_cache_mem_size > MAX_CACHE_MEMSIZE || cache_table.size > MAX_CACHE_ENTRIES)
		cache_table_rebuild();
	csp = (cache_entry *)malloc(ICACHE_SIZE + object->len);
	csp->obj.addr = (char *)csp + ICACHE_SIZE;
	csp->refcnt = csp->zb_refcnt = 0;
	csp->src = *src;
	csp->obj.len = object->len;
	memcpy(csp->obj.addr, object->addr, object->len);
	((ihdtyp *)(csp->obj.addr))->indce = csp;	/* Set backward link to this cache entry */
	added = add_hashtab_objcode(&cache_table, &csp->src, csp, &tabent);
	assert(added);

	/* Do address fixup on the literals that preceed the code */
	fixup_cnt = ((ihdtyp *)(csp->obj.addr))->fixup_vals_num;
	if (fixup_cnt)
	{
		/* Do address fixups for literals in indirect code. This is done by making them point
		   to the literals that are still resident in the stringpool. The rest of the old object
		   code will be garbage collected but these literals will be salvaged. The reason to point
		   to the stringpool version instead of in the copy we just created is that if an assignment
		   to a local variable from an indirect string literal were to occur, only the mval is copied.
		   So then there would be a local variable mval pointing into our malloc'd storage instead of
		   the stringpool. If the cache entry were recycled to hold a different object, the local
		   mval would then be pointing at garbage. By pointing these literals to their stringpool
		   counterparts, we save having to (re)copy them to the stringpool where they will be handled
		   safely and correctly.
		*/
		fix_base = (mval *)((unsigned char *)csp->obj.addr + ((ihdtyp *)(csp->obj.addr))->fixup_vals_off);
		for (fix = fix_base, i = 0 ;  i < fixup_cnt ;  i++, fix++)
		{
			if (MV_IS_STRING(fix))		/* if string, place in string pool */
				fix->str.addr = (int4)fix->str.addr + object->addr;
		}
	}
	fixup_cnt = ((ihdtyp *)(csp->obj.addr))->vartab_len;
	if (fixup_cnt)
	{
		/* Do address fix up of local variable name which is in stringpool */
		var_base = (var_tabent *)((unsigned char *)csp->obj.addr + ((ihdtyp *)(csp->obj.addr))->vartab_off);
		for (varent = var_base, i = 0; i < fixup_cnt; i++, varent++)
			varent->var_name.addr = (int4)varent->var_name.addr + object->addr;
	}
	*object = csp->obj;				/* Update location of object code for comp_indr */
	cacheflush(csp->obj.addr, csp->obj.len, BCACHE);
}
