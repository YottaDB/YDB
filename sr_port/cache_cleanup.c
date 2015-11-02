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
#include "objlabel.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "cache_cleanup.h"

GBLREF hash_table_objcode	cache_table;
GBLREF	int			indir_cache_mem_size;

void cache_cleanup(stack_frame *sf)
{
	ihdtyp		*irtnhdr;
	cache_entry	*csp;
	INTPTR_T	*vp;
	boolean_t	deleted;

	assert(sf->ctxt);
	vp = (INTPTR_T *)sf->ctxt;
	vp--;
	if ((GTM_OMAGIC << 16) + OBJ_LABEL == *vp)	/* Validate backward linkage */
	{	/* Frame is one of ours */
		vp--;
		irtnhdr = (ihdtyp *)((char *)vp + *vp);
		csp = irtnhdr->indce;
		assert(NULL != csp);
		assert(0 < csp->refcnt);
		csp->refcnt--;		/* This usage of this cache entry is done */
		/* We want to keep the entry around with the hope that it will be accessed again.
		 * When we keep too many entries or entries are using too much memory cache_put will call cache_table_rebuild()
		 * to make space removing elements with csp->refcnt == 0 and csp->zb_refcnt == 0
		 */
	} else
		GTMASSERT;			/* Not sure when this could happen */
}
