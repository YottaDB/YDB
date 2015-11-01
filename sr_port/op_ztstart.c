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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "op.h"

#define JNL_FENCE_MAX_LEVELS	255

error_def(ERR_TPMIXUP);
error_def(ERR_TRANSNEST);

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	short			dollar_tlevel;


void	op_ztstart(void)
{
	if (dollar_tlevel != 0)
		rts_error(VARLSTCNT(4) ERR_TPMIXUP, 2, "A fenced logical", "an M");

	if (jnl_fence_ctl.level >= JNL_FENCE_MAX_LEVELS)
		rts_error(VARLSTCNT(1) ERR_TRANSNEST);

	if (jnl_fence_ctl.level == 0)
	{
		jnl_fence_ctl.region_count = 0;
		jnl_fence_ctl.fence_list = (sgmnt_addrs *)-1;
	}

	++jnl_fence_ctl.level;
}
