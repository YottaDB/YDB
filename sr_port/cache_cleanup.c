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
#include "masscomp.h"
#include "cache.h"
#include "rtnhdr.h"
#include "stack_frame.h"

#ifdef DEBUG
GBLREF int	cache_temp_cnt;
#define DBG_DECR_CNT(x) --x
#else
#define DBG_DECR_CNT(x)
#endif

void cache_cleanup(stack_frame *sf)
{
	ihdtyp	*irtnhdr;
	int4	*vp;

	assert(sf->ctxt);
	vp = (int4 *)sf->ctxt;
	vp--;
	if ((OMAGIC << 16) + STAMP13 == *vp)	/* Validate backward linkage */
	{	/* Frame is one of ours */
		vp--;
		irtnhdr = (ihdtyp *)((char *)vp + *vp);
		assert(0 < irtnhdr->indce->refcnt);
		irtnhdr->indce->refcnt--;	/* This usage of this cache entry is done */
		if (0 == irtnhdr->indce->refcnt && irtnhdr->indce->temp_elem)
		{	/* Temp element to be freed */
			dqdel(irtnhdr->indce, linkq);
			dqdel(irtnhdr->indce, linktemp);
			if (irtnhdr->indce->obj.len)
				free(irtnhdr->indce->obj.addr);
			free(irtnhdr->indce);
			DBG_DECR_CNT(cache_temp_cnt);
			assert(cache_temp_cnt);
		}
	} else
		GTMASSERT;			/* Not sure when this could happen */
}
