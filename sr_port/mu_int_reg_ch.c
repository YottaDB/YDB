/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
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
#include "error.h"
#include "util.h"
#ifdef UNIX
#include "ftok_sems.h"
GBLREF gd_region	*gv_cur_region;
#endif

GBLREF bool	region;
GBLREF uint4	mu_int_errknt;
GBLREF uint4	mu_int_skipreg_cnt;

error_def(ERR_ASSERT);
error_def(ERR_DBFILERR);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);

CONDITION_HANDLER(mu_int_reg_ch)
{
	sgmnt_addrs	*csa;

	START_CH(TRUE);
	mu_int_skipreg_cnt++;
	if (DUMPABLE)
		NEXTCH;
	PRN_ERROR;
	/* After this condition handler returns (by unwinding "mu_int_reg"), its caller "mupip_integ" moves on to the next
	 * region, so release crit if the error occurred while holding it on this region (e.g. DBFLCORRP from "grab_crit"
	 * when "region_freeze" or "ss_initiate" grab crit on a database marked corrupt).
	 */
	if ((NULL != gv_cur_region) && gv_cur_region->open)
	{
		csa = &FILE_INFO(gv_cur_region)->s_addrs;
		if (csa->now_crit)
			rel_crit(gv_cur_region);
	}
	UNWIND(NULL,NULL);
}
