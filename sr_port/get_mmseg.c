/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>

#include "mmseg.h"

GBLREF	sm_uc_ptr_t	min_mmseg;
GBLREF	sm_uc_ptr_t	max_mmseg;
GBLREF	mmseg		*mmseg_head;

OS_PAGE_SIZE_DECLARE

/* === get virtual address if available === */
caddr_t get_mmseg(size_t size)
{
	mmseg			*curr, *newone;
	sm_uc_ptr_t		begin_hint, end_hint;

#if defined(__osf__) && defined(__alpha)
	/* caddr_t should be 64 bits */
	assert(8 == SIZEOF(caddr_t));
#endif

	if (!mmseg_head)
	{
#if defined(__osf__) && defined(__alpha)
		/* Some simple initial value for Tru64 UNIX, these numbers
		 * really should be from testing
		 */
		min_mmseg = (sm_uc_ptr_t)0x4000000000L;
		max_mmseg = (sm_uc_ptr_t)0x3E000000000L;
#endif

		return (caddr_t)min_mmseg;
	}

	if ((mmseg_head->begin > min_mmseg) && (size < mmseg_head->begin - min_mmseg))
		return (caddr_t)min_mmseg;

	curr = mmseg_head;
	while (curr)
	{
		end_hint = curr->next ? (sm_uc_ptr_t)curr->next->begin : max_mmseg;
		end_hint = (sm_uc_ptr_t)(ROUND_DOWN2((long)end_hint, OS_PAGE_SIZE));
		begin_hint = (sm_uc_ptr_t)(ROUND_UP2((long)(curr->end), OS_PAGE_SIZE));
		if (((unsigned long)begin_hint > (unsigned long)min_mmseg) &&
			((unsigned long)size < (unsigned long)end_hint - (unsigned long)begin_hint))
			return (caddr_t)begin_hint;
		curr = curr->next;
	}

	/* Our managable virtual address space is used up, let system decide */
	return (caddr_t)NULL;
}
