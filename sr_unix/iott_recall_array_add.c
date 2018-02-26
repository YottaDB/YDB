/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gt_timer.h"
#include "io.h"
#include "iottdef.h"
#include "comline.h"

/* Add a line of input into "tt_ptr->recall_array" history */
void	iott_recall_array_add(d_tt_struct *tt_ptr, int nchars, int width, int bytelen, void *ptr)
{
	int		recall_index, prev_recall_index;
	recall_ctxt_t	*recall, *prev_recall;

	assert(bytelen >= nchars);
	if (!bytelen)
		return;	/* no point adding NULL strings to history */
	if (NULL == tt_ptr->recall_array)
	{	/* Allocate "recall_array" first */
		recall = malloc(MAX_RECALL * SIZEOF(recall_ctxt_t));
		memset(recall, 0, MAX_RECALL * SIZEOF(recall_ctxt_t));
		assert(0 == tt_ptr->recall_index);
		tt_ptr->recall_array = recall;
	}
	recall_index = tt_ptr->recall_index;
	assert(0 <= recall_index);
	assert(MAX_RECALL > recall_index);
	recall = &tt_ptr->recall_array[recall_index];
	prev_recall_index = (0 == recall_index) ? (MAX_RECALL - 1) : (recall_index - 1);
	prev_recall = &tt_ptr->recall_array[prev_recall_index];
	if ((bytelen == prev_recall->bytelen) && !memcmp(ptr, prev_recall->buff, bytelen))
		return;	/* No point adding duplicate history lines. Return */
	if (NULL != recall->buff)
		free(recall->buff);
	recall->nchars = nchars;
	recall->width = width;
	recall->bytelen = bytelen;
	recall->buff = malloc(bytelen);
	memcpy(recall->buff, ptr, bytelen);
	if (MAX_RECALL <= ++recall_index)
		recall_index = 0;
	tt_ptr->recall_index = recall_index;
	return;
}
