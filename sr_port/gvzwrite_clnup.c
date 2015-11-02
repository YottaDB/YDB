/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "zwrite.h"
#include "error.h"
#include "change_reg.h"
#include "gvzwrite_clnup.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gvzwrite_datablk	*gvzwrite_block;
GBLREF gd_binding	*gd_map;
GBLREF gd_binding	*gd_map_top;

void	gvzwrite_clnup(void)
{
	gv_key		*old;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gv_cur_region = gvzwrite_block->gd_reg;
	change_reg();
	assert(reset_gv_target == ((gv_namehead *)gvzwrite_block->old_targ));
	if (NULL != gvzwrite_block->old_key)
	{
		old = (gv_key *)gvzwrite_block->old_key;
		memcpy(&gv_currkey->base[0], &old->base[0], old->end + 1);
		gv_currkey->end = old->end;
		gv_currkey->prev = old->prev;
		gd_map = gvzwrite_block->old_map;
		gd_map_top = gvzwrite_block->old_map_top;
		free(gvzwrite_block->old_key);
		gvzwrite_block->old_key = gvzwrite_block->old_targ = (unsigned char *)NULL;
		gvzwrite_block->subsc_count = 0;
		TREF(gv_last_subsc_null) = gvzwrite_block->gv_last_subsc_null;
		TREF(gv_some_subsc_null) = gvzwrite_block->gv_some_subsc_null;
	}
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
}
