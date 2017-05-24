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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "dse.h"
#ifdef UNIX
# include "gtm_stdio.h"
#endif

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short		crash_count;

error_def(ERR_DBRDONLY);
error_def(ERR_DSEINVLCLUSFN);
error_def(ERR_DSEONLYBGMM);
error_def(ERR_DSEWCINITCON);

void dse_wcreinit (void)
{
	unsigned char	*c;
	uint4		large_block;
	boolean_t	was_crit;
#	ifdef UNIX
	char		*fgets_res;
#	endif

        if (gv_cur_region->read_only)
                rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));

	if (cs_addrs->hdr->clustered)
	{
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEINVLCLUSFN);
		return;
	}
	if (cs_addrs->critical)
		crash_count = cs_addrs->critical->crashcnt;
	GET_CONFIRM_AND_HANDLE_NEG_RESPONSE
	if (!IS_CSD_BG_OR_MM(cs_addrs->hdr))
	{
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DSEONLYBGMM, 2, LEN_AND_LIT("WCINIT"));
		return;
	}
	was_crit = cs_addrs->now_crit;
	if (!was_crit)
		grab_crit_encr_cycle_sync(gv_cur_region);
	DSE_WCREINIT(cs_addrs);
	if (!was_crit)
		rel_crit (gv_cur_region);

	return;
}
