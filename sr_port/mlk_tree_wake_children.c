/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mlk_tree_wake_children.h"
#include "mlk_wake_pending.h"

int mlk_tree_wake_children(mlk_ctldata_ptr_t ctl,
			    mlk_shrblk_ptr_t d,
			    gd_region *reg)
{
	/* Note: should add infinite loop check */
	mlk_shrblk_ptr_t 	b, bhead, bnext;
	bool			gotone, gotone_in_subtree, delete_status;
	int4			lcnt = 0, max_loop_tries;

	gotone = FALSE;

	max_loop_tries = ((sm_uc_ptr_t)R2A(ctl->subtop) - (sm_uc_ptr_t)ctl) / sizeof(mlk_shrblk);
		/* although more than the actual, it is better than underestimating */

	for (bhead = b = d , bnext = NULL; bnext != bhead && max_loop_tries > lcnt; lcnt++)
	{
		delete_status = FALSE;
		gotone_in_subtree = FALSE;
		if (b->children)
			gotone_in_subtree = mlk_tree_wake_children(ctl, (mlk_shrblk_ptr_t)R2A(b->children), reg);

		bnext = (mlk_shrblk_ptr_t)R2A(b->rsib);
		if (!gotone_in_subtree && b->pending)
		{
			mlk_wake_pending(ctl, b, reg);
			gotone = TRUE;
		}
		else
			delete_status = mlk_shrblk_delete_if_empty(ctl, b);

		if (delete_status && b == bhead)
		{
			bhead = b = (b == bnext) ? NULL : bnext;
			bnext = NULL;
		}
		else
			b = bnext;

		gotone |= gotone_in_subtree;
	}
	return gotone;
}
