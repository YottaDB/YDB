/****************************************************************
 *								*
 * Copyright (c) 2001-2011 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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

	if (NULL == sf->ctxt)
	{	/* Possible for example if there is a runtime error and as part of compiling the error handler ($ETRAP)
		 * code, there is yet another error. In that case, "trans_code_cleanup()" would have called
		 * IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK (which in turn invokes the "cache_cleanup()" function)
		 * and would have reset "sf->ctxt" to NULL (actually to "GTM_CONTEXT(pseudo_ret)").
		 * In that case, return right away as "cache_cleanup()" already happened for this stack frame.
		 * See https://gitlab.com/YottaDB/DB/YDB/-/issues/860#note_933316079 for an example test program.
		 */
		return;
	}
	vp = (INTPTR_T *)sf->ctxt;
	vp--;
	if ((YDB_OMAGIC << 16) + OBJ_LABEL == *vp)	/* Validate backward linkage */
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
