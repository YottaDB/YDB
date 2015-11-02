/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

GBLREF gd_region 	*gv_cur_region;
GBLREF tp_region 	*grlist;

CONDITION_HANDLER(mu_freeze_ch)
{
	tp_region	*rptr1;

	START_CH;
	for (rptr1 = grlist ; rptr1 != NULL; rptr1 = rptr1->fPtr)
	{
		gv_cur_region = rptr1->reg;
		if (!gv_cur_region->open)
			continue;
		region_freeze(gv_cur_region, FALSE, FALSE, FALSE);
	}
	NEXTCH; /* should do PRN_ERROR for us */
}
