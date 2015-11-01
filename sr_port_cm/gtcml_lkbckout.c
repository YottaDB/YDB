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
#include "locklits.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_bckout.h"

GBLREF connection_struct *curr_entry;

void gtcml_lkbckout(cm_region_list *reg_list)
{
	mlk_pvtblk 	*mlk_pvt;
	unsigned char action, *ptr;
	unsigned short i, lks;

	ptr = curr_entry->clb_ptr->mbf;
	ptr++; /* jump over header */
	if (*ptr == INCREMENTAL)
		action = INCREMENTAL;
	else if (*ptr == CM_ZALLOCATES)
		action = ZALLOCATED;
	else
	{	assert(*ptr == CM_LOCKS);
		action = LOCKED;
	}

	lks = reg_list->lks_this_cmd;
	mlk_pvt = reg_list->lockdata;

	for (i = 0;i < lks; i++)
	{
		if (mlk_pvt->granted)
			mlk_bckout(mlk_pvt, action);
		mlk_pvt = mlk_pvt->next;
	}
	reg_list->oper &= ~COMPLETE;

}
