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

GBLREF mlk_pvtblk *mlk_cm_root;
GBLREF unsigned short cm_cmd_lk_ct;
GBLREF connection_struct *curr_entry;

bool gtcmtr_lkdelete(void)
{
	cm_region_list *reg_walk;
	unsigned char *ptr, laflag;
	unsigned short top,len;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKDELETE);
	ptr++;
	laflag = *ptr;
	assert (laflag == INCREMENTAL || laflag == CM_ZALLOCATES);
	if (curr_entry->new_msg)
	{
		gtcml_lkhold();
		gtcml_lklist();
	}

	reg_walk = curr_entry->region_root;
	while (reg_walk)
	{
		reg_walk->reqnode = FALSE;
		if (reg_walk->oper == PENDING)
		{	mlk_cm_root = reg_walk->lockdata;
			cm_cmd_lk_ct = reg_walk->lks_this_cmd;
			if (laflag == CM_ZALLOCATES)
				gtcml_zdeallocate();
			else
				gtcml_decrlock();
			reg_walk->lockdata = mlk_cm_root;
		}
		reg_walk = reg_walk->next;
	}
	*curr_entry->clb_ptr->mbf = CMMS_M_LKDELETED;
	curr_entry->clb_ptr->cbl = 1;
	return TRUE;
}
