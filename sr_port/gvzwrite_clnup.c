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

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gvzwrite_struct	gvzwrite_block;
GBLREF gd_binding	*gd_map;
GBLREF gd_binding	*gd_map_top;

void	gvzwrite_clnup(void)
{
	gv_key		*old;

	assert(reset_gv_target == ((gv_namehead *)gvzwrite_block.old_targ));
	RESET_GV_TARGET;
	if (gv_target)
	{
		gv_cur_region = gv_target->gd_reg;
		change_reg();
	}
	if (NULL != gvzwrite_block.old_key)
	{
		old = (gv_key *)gvzwrite_block.old_key;
		memcpy(&gv_currkey->base[0], &old->base[0], old->end + 1);
		gv_currkey->end = old->end;
		gv_currkey->prev = old->prev;
		gd_map = gvzwrite_block.old_map;
		gd_map_top = gvzwrite_block.old_map_top;
		free(gvzwrite_block.old_key);
		gvzwrite_block.old_key = gvzwrite_block.old_targ = (unsigned char *)NULL;
		gvzwrite_block.subsc_count = 0;
	}
}
