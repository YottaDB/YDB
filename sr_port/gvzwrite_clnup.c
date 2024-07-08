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

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "error.h"
#include "change_reg.h"
#include "gvzwrite_clnup.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "io.h"
#include "gvt_inline.h"

GBLREF gd_region		*gv_cur_region;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target, *reset_gv_target;
GBLREF gvzwrite_datablk	*gvzwrite_block;

void	gvzwrite_clnup(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gv_cur_region = gvzwrite_block->ref_reg;
	change_reg();
	assert(reset_gv_target == gvzwrite_block->ref_targ);
	if (NULL != gvzwrite_block->ref_key)
	{	/* see cmments in zwrite.h for more info on this section */
		COPY_KEY(gv_currkey, gvzwrite_block->ref_key);
		free(gvzwrite_block->ref_key);		/* allocated via GVKEY_INIT in gvzwr_fini; released as big and infrequent */
		gvzwrite_block->ref_key = NULL;
		gvzwrite_block->ref_targ = NULL;
		gvzwrite_block->subsc_count = 0;
		TREF(gv_last_subsc_null) = gvzwrite_block->gv_last_subsc_null;
		TREF(gv_some_subsc_null) = gvzwrite_block->gv_some_subsc_null;
	}
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);		/* this is the thing that actually restores gv_target */
}
