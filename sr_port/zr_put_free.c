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

#include "gtm_string.h"

#include "cache.h"
#include "rtnhdr.h"
#include "zbreak.h"
#include "inst_flush.h"
#include "private_code_copy.h"

GBLREF int		cache_temp_cnt;

void zr_put_free(z_records *zrecs, zbrk_struct *z_ptr)
{
	mstr	rtn_str;
	int	rtn_len;
	rhdtyp	*routine;

	assert(zrecs->beg  <= zrecs->free);
	assert(zrecs->free < zrecs->end);
	assert(z_ptr >= zrecs->beg);
	assert(z_ptr <= zrecs->free);
	if (NULL != z_ptr->action)
	{
		if (z_ptr->action->refcnt)
		{	/* This frame is active. Mark it as temp so gets released when sf is unwound */
			z_ptr->action->temp_elem = TRUE;
			DBG_INCR_CNT(cache_temp_cnt);
		} else
		{
			if (NULL != z_ptr->action->obj.addr)
				free(z_ptr->action->obj.addr);
			free(z_ptr->action);
		}
	}
	*z_ptr->mpc = z_ptr->m_opcode;
	inst_flush(z_ptr->mpc, sizeof(*z_ptr->mpc));
#ifdef USHBIN_SUPPORTED
		if ((z_ptr       == zrecs->beg  || 0 != memcmp((z_ptr - 1)->rtn.c, z_ptr->rtn.c, sizeof(mident))) &&
		    ((z_ptr + 1) == zrecs->free || 0 != memcmp((z_ptr + 1)->rtn.c, z_ptr->rtn.c, sizeof(mident))))
		{ /* No more breakpoints in the routine we just removed a break from. */
		  /* Note that since zrecs is sorted based on mpc, all breakpoints in a given routine are bunched together.
		   * Hence, it is possible to determine if all breakpoints are deleted from a routine by checking the preceding
		   * and succeeding entries of the one we are removing. */
			for (rtn_len = sizeof(mident); 0 < rtn_len && '\0' == z_ptr->rtn.c[rtn_len - 1]; rtn_len--);
			assert(0 != rtn_len);
			rtn_str.len = rtn_len;
			rtn_str.addr = z_ptr->rtn.c;
			routine = find_rtn_hdr(&rtn_str);
			if (NULL != routine->shlib_handle) /* don't need the private copy any more, revert back to shared copy */
				release_private_code_copy(routine);
		}
#endif
	zrecs->free--;
	memmove((char *)z_ptr, (char *)(z_ptr + 1), (zrecs->free - z_ptr) * sizeof(zbrk_struct)); /* potentially overlapped memory,
										 	           * use memmove, not memcpy */
	if (zrecs->free == zrecs->beg)
	{ /* all breaks gone, free space allocated for breakpoints */
		free(zrecs->beg);
		zrecs->beg = NULL;
		zrecs->free = NULL;
		zrecs->end = NULL;
	}
	return;
}
