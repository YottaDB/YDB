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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

GBLREF connection_struct *curr_entry;

void gtcml_lkhold(void)
{
	cm_region_list *reg_walk;
	mlk_pvtblk *lk_walk,*lk_walk1;

	reg_walk = curr_entry->region_root;
	while (reg_walk)
	{
		if (!reg_walk->reqnode)
		{	reg_walk->reqnode = TRUE;
			reg_walk->lks_this_cmd = 0;
			reg_walk->oper = 0;
			lk_walk = lk_walk1 = reg_walk->lockdata;
			while (lk_walk)
			{
				if (!lk_walk->granted)	/* if entry was never granted, */
				{
					if (lk_walk == reg_walk->lockdata)
					{	reg_walk->lockdata = lk_walk->next;
						free(lk_walk);
						lk_walk = lk_walk1 = reg_walk->lockdata;
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
		}
		reg_walk = reg_walk->next;
	}
}
