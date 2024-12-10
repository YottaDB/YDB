/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "interlock.h"
#include "gdsfhead.h"
#include "gdsbt.h"
#include "filestruct.h"
#include "error.h"
#include "mu_gv_cur_reg_init.h"
#include "db_ipcs_reset.h"

GBLREF bool		region;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		process_id;

CONDITION_HANDLER(mu_int_ch)
{
	gd_region *stats_reg = NULL;
	sgmnt_addrs *stats_csa = NULL;
	node_local_ptr_t stats_nl = NULL;
	unix_db_info	*stats_udi = NULL;

	START_CH(TRUE);
	if (gv_cur_region)
	{
		if (IS_STATSDB_REG(gv_cur_region))
			stats_reg = gv_cur_region;
		else
			BASEDBREG_TO_STATSDBREG(gv_cur_region, stats_reg);
	}
	if (stats_reg)
		stats_udi = FILE_INFO(stats_reg);
	if (stats_udi)
		stats_csa = &stats_udi->s_addrs;
	if (stats_csa)
		stats_nl = stats_csa->nl;
	if (stats_nl && GLOBAL_LATCH_HELD_BY_US(&stats_nl->statsdb_field_latch))
	{
		rel_latch(&stats_nl->statsdb_field_latch);
	}
	if (!region)
	{
		db_ipcs_reset(gv_cur_region);
		mu_gv_cur_reg_free(); /* mu_gv_cur_reg_init was done in mu_int_init() */
	}
	NEXTCH;
}
