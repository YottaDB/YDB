/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "jnl.h"		/* needed for the WCSFLU_* macros */
#include "wcs_flu.h"
#include "dse.h"

GBLREF gd_region	*gv_cur_region;
GBLREF short		crash_count;
GBLREF sgmnt_addrs	*cs_addrs;

error_def(ERR_DBRDONLY);
error_def(ERR_DSEONLYBGMM);

void dse_flush(void)
{
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));

	switch (gv_cur_region->dyn.addr->acc_meth)
	{
	case dba_bg:
	case dba_mm:
		if (cs_addrs->critical)
			crash_count = cs_addrs->critical->crashcnt;
		wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
		break;
	default:
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DSEONLYBGMM, 2, LEN_AND_LIT("BUFFER_FLUSH"));
		break;
	}
	return;
}
