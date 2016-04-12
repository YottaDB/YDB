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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "locklits.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "mlkdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_unpend.h"

GBLREF mlk_pvtblk		*mlk_cm_root;
GBLREF unsigned short		cm_cmd_lk_ct;
GBLREF connection_struct	*curr_entry;

void gtcml_lkrundown(void)
{
	cm_region_list	*reg_walk;
	unsigned char	*ptr, laflag;
	unsigned short	top,len;
	uint4		status;

	cancel_timer((TID)curr_entry);	/* Cancel any outstanding lock starvation timer */
	reg_walk = curr_entry->region_root;
	curr_entry->state = 0;
	while (reg_walk)
	{
		reg_walk->reqnode = FALSE;
		if (reg_walk->lockdata)
		{
			if (reg_walk->blkd)
			{
				mlk_unpend(reg_walk->blkd);
				reg_walk->blkd = 0;
			}
			mlk_cm_root = reg_walk->lockdata;
			cm_cmd_lk_ct = 0;
			gtcml_unlock();
			gtcml_zdeallocate();
			reg_walk->lockdata = mlk_cm_root;
		}
		reg_walk = reg_walk->next;
	}
}
