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
#include "gtm_string.h"
#include "mdq.h"
#include "cache.h"
#include "rtnhdr.h"
#include "cachectl.h"
#include "cacheflush.h"

GBLREF cache_entry	*cache_entry_base, *cache_entry_top, *cache_hashent, *cache_stealp, cache_temps;
GBLREF int		cache_temp_cnt;

void cache_put(unsigned char code, mstr *source, mstr *object)
{
	cache_entry	*csp;
	int		trips, fixup_cnt, i;
	mval		*fix_base, *fix;

	for (csp = cache_stealp, trips = -1; ; csp++)
	{
		if (csp == cache_stealp)
		{	/* Allow two complete trips through. Try to find reusable entry */
			if (0 < trips++)
			{	/* No reusable entry was found. Create a temporary extention entry.
				   This entry will be freed when stackframe is unwound
				*/
				csp = (cache_entry *)malloc(sizeof(*csp));
				memset((char *)csp, 0, sizeof(*csp));
				dqins(&cache_temps, linktemp, csp);
				DBG_INCR_CNT(cache_temp_cnt);
				csp->temp_elem = TRUE;
				break;
			}
		}
		/* Note that recycling of csp back to beginning occurs after the equality test
		   above since cache_stealp may have been left "un-normalized". */
		if (csp >= cache_entry_top)
			csp = cache_entry_base;
		if (csp->referenced)
		{	/* Don't do referenced frames till 2nd pass.*/
			csp->referenced = FALSE;
			continue;
		}
		/* If frame is in use, we will ignore the frame */
		if (0 == csp->refcnt)
			break;
	}
	/* We have a reusable entry in csp */
	if (!csp->temp_elem)	/* only update clock ptr if element in perm cache */
		cache_stealp = csp + 1;
	if (NULL != csp->linkq.fl)
		dqdel(csp, linkq);
	dqins(cache_hashent, linkq, csp);

	/* If a buffer exists and is big enough, use it, else replace it */
	if (0 != csp->obj.len && csp->real_obj_len < object->len)
	{
		free(csp->obj.addr);
		csp->obj.len = 0;
	}
	if (0 == csp->obj.len)
	{
		csp->obj.addr = (char *)malloc(object->len);
		csp->real_obj_len = object->len;
	}
	csp->code = code;
	csp->src.len = source->len;
	csp->src.addr = source->addr;
	csp->obj.len = object->len;
	memcpy(csp->obj.addr, object->addr, object->len);
	((ihdtyp *)(csp->obj.addr))->indce = csp;	/* Set backward link to this cache entry */

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
		fix_base = (mval *)((unsigned char *)csp->obj.addr + ((ihdtyp *)(csp->obj.addr))->fixup_vals_ptr);
		for (fix = fix_base, i = 0 ;  i < fixup_cnt ;  i++, fix++)
		{
			if (MV_IS_STRING(fix))		/* if string, place in string pool */
				fix->str.addr = (int4)fix->str.addr + object->addr;
		}
	}
	*object = csp->obj;				/* Update location of object code for comp_indr */
	cacheflush(csp->obj.addr, csp->obj.len, BCACHE);
}
