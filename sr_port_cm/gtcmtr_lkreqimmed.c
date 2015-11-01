/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mlkdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "locklits.h"
#include "gtcmtr_protos.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_unpend.h"

GBLREF connection_struct *curr_entry;
GBLREF mlk_pvtblk *mlk_cm_root;

bool gtcmtr_lkreqimmed(void)
{
	unsigned char *ptr, return_val;
	cm_region_list *reg_walk;
	mlk_pvtblk *lk_walk,*lk_walk1;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKREQIMMED);
	ptr++;
	ptr++;
	curr_entry->transnum = *ptr;
	curr_entry->last_cancelled = CM_NOLKCANCEL;

	if (curr_entry->new_msg)
	{
		gtcml_lkhold();
		gtcml_lklist();
	}
	return_val = gtcml_dolock();

	if (return_val == CMLCK_REQUEUE)
		return CM_NOOP;

	if (return_val == CMMS_M_LKBLOCKED)
	{	/* list has already been backed out, so entries not held must be freed */
		return_val = CMMS_M_LKABORT;
		reg_walk = curr_entry->region_root;
		while (reg_walk)
		{
			if (reg_walk->oper == PENDING)
			{
				if (reg_walk->blkd)
				{
					mlk_unpend(reg_walk->blkd);
					reg_walk->blkd = 0;
				}
				reg_walk->oper = 0;
				reg_walk->lks_this_cmd = 0;
				lk_walk = lk_walk1 = mlk_cm_root = reg_walk->lockdata;
				while (lk_walk)
				{
					if (!(lk_walk->granted))		/* if entry was never granted, */
					{
						if (mlk_cm_root == lk_walk)
						{	mlk_cm_root = lk_walk->next;
							free(lk_walk);
							lk_walk = lk_walk1 = mlk_cm_root;
						}
						else
						{
							lk_walk1->next = lk_walk->next;
							free(lk_walk);
							lk_walk = lk_walk1->next;
						}
					} /* delete list entry */
					else
					{
						lk_walk1 = lk_walk;
						lk_walk = lk_walk->next;
					}
				}
				reg_walk->lockdata = mlk_cm_root;
			}
			reg_walk = reg_walk->next;
		}
	}

	*curr_entry->clb_ptr->mbf = return_val;
	curr_entry->clb_ptr->cbl  = 1;
	return TRUE;
}
