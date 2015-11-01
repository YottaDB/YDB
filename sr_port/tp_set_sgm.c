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
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "min_max.h"
#include "tp_set_sgm.h"

GBLDEF	sgm_info	*sgm_info_ptr;
GBLDEF	tp_region	*tp_reg_free_list;	/* Ptr to list of tp_regions that are unused */
GBLDEF  tp_region	*tp_reg_list;		/* Ptr to list of tp_regions for this transaction */

GBLREF	short		crash_count;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgm_info	*first_sgm_info;

void tp_set_sgm(void)
{
	sgm_info	*si;

	si = ((sgmnt_addrs *)&FILE_INFO(gv_cur_region)->s_addrs)->sgm_info_ptr;
	if (si->fresh_start)
	{
		si->next_sgm_info = first_sgm_info;
		first_sgm_info = si;
		si->start_tn = cs_addrs->ti->curr_tn;
		if (cs_addrs->critical)
			si->crash_count = cs_addrs->critical->crashcnt;
		insert_region(gv_cur_region, &tp_reg_list, &tp_reg_free_list, sizeof(tp_region));
		si->fresh_start = FALSE;
	}
	sgm_info_ptr = si;
	crash_count = si->crash_count;
}
