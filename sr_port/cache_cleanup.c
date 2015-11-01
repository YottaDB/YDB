/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mdq.h"
#include "objlabel.h"
#include "cache.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "cache_cleanup.h"

GBLREF int	cache_temp_cnt;

void cache_cleanup(stack_frame *sf)
{
	ihdtyp		*irtnhdr;
	cache_entry	*indce;
	int4		*vp;

	assert(sf->ctxt);
	vp = (int4 *)sf->ctxt;
	vp--;
	if ((GTM_OMAGIC << 16) + OBJ_LABEL == *vp)	/* Validate backward linkage */
	{	/* Frame is one of ours */
		vp--;
		irtnhdr = (ihdtyp *)((char *)vp + *vp);
		indce = irtnhdr->indce;
		assert(0 < indce->refcnt);
		indce->refcnt--;	/* This usage of this cache entry is done */
		if (0 == indce->refcnt && indce->temp_elem)
		{	/* Temp element to be freed */
			if (indce->linkq.fl)
			{	/* Cache entries orphaned by op_setzbrk won't be on queues */
				dqdel(indce, linkq);
			}
			if (indce->linktemp.fl)
			{
				dqdel(indce, linktemp);
			}
			if (indce->obj.len)
				free(indce->obj.addr);
			free(indce);
			assert(cache_temp_cnt);
			DBG_DECR_CNT(cache_temp_cnt);
		}
	} else
		GTMASSERT;			/* Not sure when this could happen */
}
