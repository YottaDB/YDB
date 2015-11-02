/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "rtnhdr.h"
#include "zbreak.h"
#include "inst_flush.h"
#include "private_code_copy.h"

#ifdef __ia64
#include "ia64.h"
#endif /*__ia64__*/


GBLREF hash_table_objcode	cache_table;

void zr_put_free(z_records *zrecs, zbrk_struct *z_ptr)
{
	mstr		rtn_str;
	rhdtyp		*routine;
	boolean_t	deleted;

	assert(zrecs->beg  <= zrecs->free);
	assert(zrecs->free < zrecs->end);
	assert(z_ptr >= zrecs->beg);
	assert(z_ptr <= zrecs->free);
	if (NULL != z_ptr->action)
	{
		assert(z_ptr->action->zb_refcnt > 0);
		z_ptr->action->zb_refcnt--;
		if (0 == z_ptr->action->zb_refcnt)
		{
			if (0 == z_ptr->action->refcnt)
			{
				deleted = delete_hashtab_objcode(&cache_table, &z_ptr->action->src);
				assert(deleted);
				free(z_ptr->action);
			}
			z_ptr->action = NULL;
		}
	}
#ifndef __ia64
	*z_ptr->mpc = z_ptr->m_opcode;
	inst_flush(z_ptr->mpc, sizeof(*z_ptr->mpc));
#else
	{
	  	ia64_fmt_A4 inst;
		EXTRACT_INST(z_ptr->mpc, inst, 3);
		zb_code imm14 = z_ptr->m_opcode;
		inst.format.imm7b=imm14;
		inst.format.imm6d=imm14 >> 7;
		inst.format.sb=imm14 >> 13;
		/*Revist when instruction bundling implemented.*/
		UPDATE_INST(z_ptr->mpc, inst, 3);
		inst_flush(z_ptr->mpc, sizeof(ia64_bundle));
	}
#endif

#ifdef USHBIN_SUPPORTED
	if (((z_ptr == zrecs->beg) || !MIDENT_EQ((z_ptr - 1)->rtn, z_ptr->rtn)) &&
	    (((z_ptr + 1) == zrecs->free) || !MIDENT_EQ((z_ptr + 1)->rtn, z_ptr->rtn)))
	{ /* No more breakpoints in the routine we just removed a break from. */
	  /* Note that since zrecs is sorted based on mpc, all breakpoints in a given routine are bunched together.
	   * Hence, it is possible to determine if all breakpoints are deleted from a routine by checking the
	   * preceding and succeeding entries of the one we are removing. */
		assert(0 != z_ptr->rtn->len);
		rtn_str.len = z_ptr->rtn->len;
		rtn_str.addr = z_ptr->rtn->addr;
		routine = find_rtn_hdr(&rtn_str);
		if (NULL != routine->shlib_handle) /* don't need the private copy any more, revert back to shared copy */
			release_private_code_copy(routine);
	}
#endif
	zrecs->free--;
	/* potentially overlapped memory, use memmove, not memcpy */
	memmove((char *)z_ptr, (char *)(z_ptr + 1), (zrecs->free - z_ptr) * sizeof(zbrk_struct));
	if (zrecs->free == zrecs->beg)
	{ /* all breaks gone, free space allocated for breakpoints */
		free(zrecs->beg);
		zrecs->beg = NULL;
		zrecs->free = NULL;
		zrecs->end = NULL;
	}
	return;
}
