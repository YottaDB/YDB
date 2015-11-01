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
#include "underr.h"

GBLREF lvzwrite_struct lvzwrite_block;

void lvzwr_arg(int t,mval *a1,mval *a2)
{
	short int i;

	i = lvzwrite_block.subsc_count++;
	/* it would be good to guard the array i < sizeof... */
	if (a1)
	{
		if (!MV_DEFINED(a1))
		{	underr(a1);
		}
		MV_FORCE_NUM(a1);
		MV_FORCE_STR(a1);
	}
	if (a2)
	{
		if (!MV_DEFINED(a2))
		{	underr(a2);
		}
		MV_FORCE_NUM(a2);
		MV_FORCE_STR(a2);
	}
	((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[i].subsc_type = t;
	((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[i].first = a1;
	((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[i].second = a2;
	if (t != ZWRITE_ASTERISK && t != ZWRITE_ALL)
		lvzwrite_block.mask |= 1 << i;

	if (t != ZWRITE_VAL)
		lvzwrite_block.fixed = FALSE;

	return;
}
