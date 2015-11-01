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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "subscript.h"
#include "mlkdef.h"
#include "zshow.h"

GBLREF lvzwrite_struct	lvzwrite_block;
GBLREF int		merge_args;

void lvzwr_init(bool t, mval *val) /*false-->mident, true-->mval*/
{
	unsigned char *ch;
	int n;
	lvzwrite_block.name_type = t;
	if (!merge_args)
	{
		MV_FORCE_STR(val);
		lvzwrite_block.pat = val;
	} else
		lvzwrite_block.pat = NULL;
	lvzwrite_block.mask = lvzwrite_block.subsc_count = 0;
	if (!lvzwrite_block.sub)
		lvzwrite_block.sub = (struct zwr_sub_lst *)malloc(sizeof(zwr_sub_lst) * MAX_LVSUBSCRIPTS);
	lvzwrite_block.fixed = TRUE;
	return;
}
