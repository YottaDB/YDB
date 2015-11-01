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
#include "underr.h"

GBLREF gvzwrite_struct gvzwrite_block;

void gvzwr_arg(int t,mval *a1,mval *a2)
{
	short int i;

	i = gvzwrite_block.subsc_count++;
	/* it would be good to guard the array i < sizeof... */
	if (a1)
	{
		if (!MV_DEFINED(a1))
			underr(a1);
		MV_FORCE_NUM(a1);
		MV_FORCE_STR(a1);
		if ((ZWRITE_VAL != t) && (0 == a1->str.len))	/* value is real - leave it alone */
			a1 = NULL;
	}
	if (a2)
	{
		if (!MV_DEFINED(a2))
			underr(a2);
		MV_FORCE_NUM(a2);
		MV_FORCE_STR(a2);
		if (0 == a2->str.len)				/* can never be value */
			a2 = NULL;
	}
	((zwr_sub_lst *)gvzwrite_block.sub)->subsc_list[i].subsc_type = t;
	((zwr_sub_lst *)gvzwrite_block.sub)->subsc_list[i].first = a1;
	((zwr_sub_lst *)gvzwrite_block.sub)->subsc_list[i].second = a2;
	if ((ZWRITE_ASTERISK != t) && (ZWRITE_ALL != t))
		gvzwrite_block.mask |= 1 << i;

	if (ZWRITE_VAL != t)
		gvzwrite_block.fixed = FALSE;
	return;
}
