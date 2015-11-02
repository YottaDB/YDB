/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

void dse_flush(void)
{
	boolean_t	was_crit;

	error_def(ERR_DSEONLYBGMM);
	error_def(ERR_DBRDONLY);

	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));

	switch (gv_cur_region->dyn.addr->acc_meth)
	{
	case dba_bg:
	case dba_mm:
		if (cs_addrs->critical)
			crash_count = cs_addrs->critical->crashcnt;
		was_crit = cs_addrs->now_crit;
		if (!was_crit)
			grab_crit(gv_cur_region);
		wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
		if (!was_crit)
			rel_crit(gv_cur_region);
		break;
	default:
		rts_error(VARLSTCNT(4) ERR_DSEONLYBGMM, 2, LEN_AND_LIT("BUFFER_FLUSH"));
		break;
	}
	return;
}
