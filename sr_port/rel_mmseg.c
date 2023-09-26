/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* === release virtual address if matched === */
void rel_mmseg(caddr_t begin)
{
	mmseg			*curr, *next;

	if (mmseg_head)
	{
		curr = mmseg_head;
		if ((sm_uc_ptr_t)begin == curr->begin)
		{
			mmseg_head = curr->next;
			free(curr);
			return;
		}
		while ((next = curr->next))
		{
			if ((sm_uc_ptr_t)begin == next->begin)
			{
				curr->next = next->next;
				free(next);
				return;
			}
			curr = next;
		}
	}
	return;
}
