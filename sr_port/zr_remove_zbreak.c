/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>
#include "zbreak.h"
#include "inst_flush.h"
#include "private_code_copy.h"
#include "gtm_text_alloc.h"

void zr_remove_zbreak(z_records *zrecs, zbrk_struct *z_ptr)
{
	mstr		rtn_str;
	rhdtyp		*routine, *rtncopy;
	boolean_t	deleted;

	assert(zrecs->beg  <= zrecs->free);
	assert(zrecs->free < zrecs->end);
	assert(z_ptr >= zrecs->beg);
	assert(z_ptr <= zrecs->free);
	if (NULL != z_ptr->action)
	{	/* An action exists, reduce our interest in it */
		assert(0 < z_ptr->action->zb_refcnt);
		z_ptr->action->zb_refcnt--;
		if (0 == z_ptr->action->zb_refcnt)
			z_ptr->action = NULL;
	}
	/* In the generated code, change the opcode in the instruction */
#	ifdef COMPLEX_INSTRUCTION_UPDATE
	EXTRACT_AND_UPDATE_INST(z_ptr->mpc, z_ptr->m_opcode);
#	else
	*z_ptr->mpc = z_ptr->m_opcode;
#	endif /* COMPLEX_INSTRUCTION_UPDATE */
	inst_flush(z_ptr->mpc, SIZEOF(INST_TYPE));
#	ifdef USHBIN_SUPPORTED
	if (((z_ptr == zrecs->beg) || ((z_ptr - 1)->rtnhdr != z_ptr->rtnhdr))
	    && (((z_ptr + 1) == zrecs->free) || ((z_ptr + 1)->rtnhdr != z_ptr->rtnhdr)))
	{	/* No more breakpoints in the routine we just removed a ZBREAK from. Note that since zrecs is sorted based
		 * on mpc, all breakpoints in a given routine are bunched together. Hence, it is possible to determine
		 * if all breakpoints are deleted from a routine by checking the preceding and succeeding entries of the
		 * one we are removing.
		 */
		assert(0 != z_ptr->rtn->len);
		rtn_str.len = z_ptr->rtn->len;
		rtn_str.addr = z_ptr->rtn->addr;
		routine = rtncopy = z_ptr->rtnhdr;
		assert(NULL != routine);
		/* We now have the routine header associated with this ZBREAK. But if this routine was recursively relinked
		 * AND shared, then the address we need to check about releasing is associated with the copy of the routine
		 * header/code we made for the recursive link (see handle_active_old_versions() for description).
		 */
		if (NULL != routine->active_rhead_adr)
		{
			rtncopy = routine->active_rhead_adr;
			rtncopy->has_ZBREAK = FALSE;
		}
		if (NULL != rtncopy->shared_ptext_adr) 		/* Revert back to shared copy of routine */
		{
			assert(rtncopy->shared_ptext_adr != rtncopy->ptext_adr);
			release_private_code_copy(rtncopy);
		}
		routine->has_ZBREAK = FALSE;	/* Indicate no more ZBREAKs in this routine */
	}
#	endif /* USHBIN_SUPPORTED */
	zrecs->free--;
	/* Potentially overlapped memory, use memmove, not memcpy */
	memmove((char *)z_ptr, (char *)(z_ptr + 1), (zrecs->free - z_ptr) * SIZEOF(zbrk_struct));
	if (zrecs->free == zrecs->beg)
	{	/* All ZBREAKS gone, free space allocated for breakpoints */
		free(zrecs->beg);
		zrecs->beg = NULL;
		zrecs->free = NULL;
		zrecs->end = NULL;
	}
	return;
}
