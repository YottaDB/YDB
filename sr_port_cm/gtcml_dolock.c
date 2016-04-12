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
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "locklits.h"
#include "gtcml.h"

GBLREF connection_struct	*curr_entry;
GBLREF mlk_pvtblk		*mlk_cm_root;
GBLREF unsigned short		cm_cmd_lk_ct;

unsigned char gtcml_dolock(void)
{
	cm_region_list	*reg_walk, *bck_out;
	unsigned char	return_val;
	unsigned char	*ptr, laflag;
	char		granted;

	ptr = curr_entry->clb_ptr->mbf;
	ptr++; /* jump over header */
	laflag = *ptr;
	return_val = CMMS_M_LKGRANTED;

	reg_walk = curr_entry->region_root;
	while (reg_walk)
	{
		reg_walk->reqnode = FALSE;
		if (reg_walk->oper & PENDING)
		{
			cm_cmd_lk_ct = reg_walk->lks_this_cmd;
			mlk_cm_root = reg_walk->lockdata;
			gtcml_lckclr();
			reg_walk->blkd = 0;
			switch(laflag)
			{
				case CM_LOCKS:
					granted = gtcml_lock(reg_walk);
					break;
				case CM_ZALLOCATES:
					granted = gtcml_zallocate(reg_walk);
					break;
				case INCREMENTAL:
					granted = gtcml_incrlock(reg_walk);
					break;
				default:
					GTMASSERT;
			}
			if (granted == GRANTED)
			{
				reg_walk->oper |= COMPLETE;
				reg_walk = reg_walk->next;
				continue;
			}
			bck_out = curr_entry->region_root;
			while (bck_out != reg_walk)
			{
				gtcml_lkbckout(bck_out);
				bck_out = bck_out->next;
			}
			if (granted != STARVED)
			{	return_val = CMMS_M_LKBLOCKED;
				break;
			}
			else
			{
				return_val = CMLCK_REQUEUE;
				break;
			}
		}
		else
			reg_walk = reg_walk->next;
	}
	return return_val;
}
