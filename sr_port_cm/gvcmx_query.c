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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gvcmx.h"
#include "gvcmz.h"
#include "mvalconv.h"

GBLREF	gd_region	*gv_cur_region;

bool gvcmx_query(mval *val)
{
	mval		temp;
	struct CLB	*lnk;

	gvcmz_doop(CMMS_Q_QUERY, CMMS_R_QUERY, &temp);
	lnk = gv_cur_region->dyn.addr->cm_blk;
	if (((link_info *)lnk->usr)->query_is_queryget)
		*val = temp;
	return (((link_info *)lnk->usr)->query_is_queryget ?
			(MV_DEFINED(&temp) ? TRUE : FALSE) : /* we return TRUE (1) to avoid int -> bool (char) lossy assignment */
			MV_FORCE_INTD(&temp));
}
