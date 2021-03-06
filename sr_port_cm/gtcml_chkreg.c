/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

GBLDEF cm_lckblkreg *blkdlist;
GBLREF gd_region *gv_cur_region;

GBLDEF ABS_TIME chkreg_time;

void gtcml_chkreg(void)
{
	cm_lckblkreg *reg, *reg1;

	ASSERT_IS_LIBGNPSERVER;
	sys_get_curr_time(&chkreg_time);	/* just once per pass */
	reg1 = reg = blkdlist;

	while (reg)
	{
		gv_cur_region = reg->region->reg;
		if (0 == reg->region->refcnt)
		{
			gtcml_chklck(reg,FALSE);
			assert(0 == reg->lock);
		} else if (reg->region->wakeup < ((mlk_ctldata *)FILE_INFO(gv_cur_region)->s_addrs.mlkctl)->wakeups)
		{
			gtcml_chklck(reg,FALSE);
			reg->region->wakeup = ((mlk_ctldata *)FILE_INFO(gv_cur_region)->s_addrs.mlkctl)->wakeups;
			reg->pass = CM_BLKPASS;
		} else if (0 == --reg->pass)
		{
			gtcml_chklck(reg,TRUE);
			reg->pass = CM_BLKPASS;
		}

		if (0 == reg->lock)
		{
			if (reg == blkdlist)
			{
				blkdlist = reg->next;
				free(reg);
				reg = reg1 = blkdlist;
			} else
			{
				reg1->next = reg->next;
				free(reg);
				reg = reg1->next;
			}
		} else
		{
			reg1 = reg;
			reg = reg->next;
		}
	}
}
