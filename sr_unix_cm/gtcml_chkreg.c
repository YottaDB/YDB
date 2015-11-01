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
#include "filestruct.h"
#include "mlkdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmlkdef.h"

GBLDEF cm_lckblkreg *blkdlist;
GBLREF gd_region *gv_cur_region;

void gtcml_chkreg()
{
	cm_lckblkreg *reg, *reg1;

	reg1 = reg = blkdlist;

	while (reg)
	{
		gv_cur_region = reg->region->reg;
		if (reg->region->refcnt == 0)
		{
			gtcml_chklck(reg,FALSE);
			assert (reg->lock == 0);
		}
		else if (reg->region->wakeup < ((mlk_ctldata *)FILE_INFO(gv_cur_region)->s_addrs.lock_addrs[0])->wakeups)
		{
			gtcml_chklck(reg,FALSE);
			reg->region->wakeup = ((mlk_ctldata *)FILE_INFO(gv_cur_region)->s_addrs.lock_addrs[0])->wakeups;
			reg->pass = CM_BLKPASS;
		}
		else if (--reg->pass == 0)
		{
			gtcml_chklck(reg,TRUE);
			reg->pass = CM_BLKPASS;
		}

		if (reg->lock == 0)
		{
			if (reg == blkdlist)
			{
				blkdlist = reg->next;
				free(reg);
				reg = reg1 = blkdlist;
			}
			else
			{
				reg1->next = reg->next;
				free(reg);
				reg = reg1->next;
			}
		}
		else
		{
			reg1 = reg;
			reg = reg->next;
		}
	}
}
