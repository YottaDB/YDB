/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "interlock.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs_ptr_t */


GBLREF gd_region 	*gv_cur_region;
GBLREF sgmnt_data	*cs_data;
GBLREF tp_region 	*grlist;
GBLREF uint4		process_id;
GBLREF jnlpool_addrs_ptr_t	jnlpool;	/* TP_CHANGE_REG */

CONDITION_HANDLER(mu_freeze_ch)
{
	tp_region	*rptr1;

	START_CH(TRUE);
	for (rptr1 = grlist ; rptr1 != NULL; rptr1 = rptr1->fPtr)
	{
		TP_CHANGE_REG(rptr1->reg);
		if (!gv_cur_region->open)
			continue;
		if (process_id == FILE_INFO(gv_cur_region)->s_addrs.nl->freeze_latch.u.parts.latch_pid)
			rel_latch(&FILE_INFO(gv_cur_region)->s_addrs.nl->freeze_latch);
		region_freeze(gv_cur_region, FALSE, FALSE, FALSE, FALSE, FALSE);
	}
	NEXTCH; /* should do PRN_ERROR for us */
}
