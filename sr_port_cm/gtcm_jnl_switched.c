/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "relqueopi.h"
#include "gtcm_jnl_switched.h"
#include "gtcm_find_reghead.h"

GBLDEF cm_region_head	*curr_cm_reg_head; /* current cm_region_head structure serviced by the GT.CM server. */

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

void gtcm_jnl_switched(gd_region *reg)
{
	cm_region_list *ptr;
	cm_region_head	*reghead;
	que_ent		qe;

	if ((NULL != curr_cm_reg_head) && (curr_cm_reg_head->reg == reg))
	{
		assert(reg == gv_cur_region);
		reghead = curr_cm_reg_head;
	} else
	{	/* curr_cm_reg_head does NOT correspond to the input region. Determine it. */
		reghead = gtcm_find_reghead(reg);
		if (NULL == reghead)
			return;
	}
	ptr = (cm_region_list *)RELQUE2PTR(reghead->head.fl);
	while ((cm_region_head *)ptr != reghead)
	{
		ptr->pini_addr = 0;
		ptr = (cm_region_list *)RELQUE2PTR(ptr->regque.fl);
	}
}
