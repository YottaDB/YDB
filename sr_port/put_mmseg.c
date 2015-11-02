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

#include "gtm_string.h"

#include <sys/types.h>

#include "mmseg.h"

GBLREF	sm_uc_ptr_t	min_mmseg;
GBLREF	sm_uc_ptr_t	max_mmseg;
GBLREF	mmseg		*mmseg_head;

OS_PAGE_SIZE_DECLARE

/* === register virtual address mmaped === */
void put_mmseg(caddr_t begin, size_t size)
{
	mmseg	*curr, *next;

	/* --- caddr_t should be 64 bits --- */
	assert(8 == SIZEOF(caddr_t));

	/* --- create the new item --- */
	curr = (mmseg *)malloc(SIZEOF(mmseg));
	memset((char *)curr, 0, SIZEOF(mmseg));
	curr->begin = (sm_uc_ptr_t)begin;
	curr->end = (sm_uc_ptr_t)begin + size;

	/* --- if the list is empty --- */
	if (!mmseg_head)
	{
		mmseg_head = curr;
		return;
	}

	/* --- new head --- */
	if ((sm_uc_ptr_t)begin < mmseg_head->begin)
	{
		/* --- no overlap allowed --- */
		if (curr->end > mmseg_head->begin)
		{
			GTMASSERT;
		}
		curr->next = mmseg_head;
		mmseg_head = curr;
		return;
	}

	next = mmseg_head;
	while (next->next && ((sm_uc_ptr_t)begin > next->next->begin))
		next = next->next;
	if (next->end > (sm_uc_ptr_t)begin)
	{
		/* --- no overlap allowed --- */
		GTMASSERT;
	}
	curr->next = next->next;
	next->next = curr;
	return;
}
