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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "subscript.h"

GBLREF gvzwrite_datablk *gvzwrite_block;

void gvzwr_init(unsigned short t, mval *val, int4 pat)
{
	if (NULL == gvzwrite_block)
	{
		gvzwrite_block = malloc(SIZEOF(gvzwrite_datablk));
		memset(gvzwrite_block, 0, SIZEOF(gvzwrite_datablk));
	}
	MV_FORCE_STR(val);
	gvzwrite_block->type = pat;
	if (NULL == gvzwrite_block->sub)
		gvzwrite_block->sub = (zwr_sub_lst *)malloc(SIZEOF(zwr_sub_lst) * MAX_GVSUBSCRIPTS);
	gvzwrite_block->pat = val;
	gvzwrite_block->mask = gvzwrite_block->subsc_count = 0;
	gvzwrite_block->fixed = TRUE;
	return;
}
