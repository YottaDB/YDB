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
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_unpend.h"

GBLREF mlk_pvtblk *mlk_cm_root;
GBLREF connection_struct *curr_entry;
GBLREF uint4 process_id;

unsigned char gtcml_lkcancel(void)
{
	cm_region_list *reg_walk;
	unsigned short i;
	mlk_pvtblk *lk_walk, *lk_walk1;

	reg_walk = curr_entry->region_root;
	while (reg_walk)
	{
		if (reg_walk->oper & PENDING)
		{
			if (reg_walk->blkd)
			{	mlk_unpend(reg_walk->blkd);
				reg_walk->blkd = 0;
			}
			else if (reg_walk->oper & COMPLETE)
				gtcml_lkbckout(reg_walk);

			lk_walk = lk_walk1 = mlk_cm_root = reg_walk->lockdata;
			i = 0;
			while (lk_walk && i++ < reg_walk->lks_this_cmd)
			{
				if (!(lk_walk->granted)) /* if entry was never granted, */
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
			reg_walk->oper = 0;
			reg_walk->lockdata = mlk_cm_root;
			reg_walk->lks_this_cmd = 0;
		}
		reg_walk = reg_walk->next;
	}
	curr_entry->lk_cancel = CM_NOLKCANCEL;
	curr_entry->state = 0;
	return CMMS_M_LKDELETED;
}
