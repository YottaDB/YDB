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

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "relqueopi.h"
#include "gtcm_jnl_switched.h"

GBLDEF cm_region_head	*curr_cm_reg_head; /* current cm_region_head structure serviced by the GT.CM server. */

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

void gtcm_jnl_switched()
{
	cm_region_list *ptr;
	que_ent		qe;

	assert(curr_cm_reg_head->reg == gv_cur_region);
	ptr = (cm_region_list *)RELQUE2PTR(curr_cm_reg_head->head.fl);
	while ((cm_region_head *)ptr != curr_cm_reg_head)
	{
		ptr->pini_addr = 0;
		ptr = (cm_region_list *)RELQUE2PTR(ptr->regque.fl);
	}
}
