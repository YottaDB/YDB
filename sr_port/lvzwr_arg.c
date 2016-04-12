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
#include "hashtab_mname.h"
#include "hashtab_addr.h"
#include "zwrite.h"

GBLREF lvzwrite_datablk *lvzwrite_block;

void lvzwr_arg(int t, mval *a1, mval *a2)
{
	int sub_idx;

	assert(lvzwrite_block);
	sub_idx = lvzwrite_block->subsc_count++;
	/* it would be good to guard the array sub_idx < sizeof... */
	if (a1)
	{
		MV_FORCE_DEFINED(a1);
		if (MV_IS_CANONICAL(a1))
			MV_FORCE_NUMD(a1);
		MV_FORCE_STRD(a1);
		if ((ZWRITE_VAL != t) && (0 == a1->str.len))	/* value is real - leave it alone */
			a1 = NULL;
	}
	if (a2)
	{
		MV_FORCE_DEFINED(a2);
		if (MV_IS_CANONICAL(a2))
			MV_FORCE_NUMD(a2);
		MV_FORCE_STRD(a2);
		if (0 == a2->str.len)				/* can never be value */
			a2 = NULL;
	}
	((zwr_sub_lst *)lvzwrite_block->sub)->subsc_list[sub_idx].subsc_type = t;
	((zwr_sub_lst *)lvzwrite_block->sub)->subsc_list[sub_idx].first = a1;
	((zwr_sub_lst *)lvzwrite_block->sub)->subsc_list[sub_idx].second = a2;
	if ((ZWRITE_ASTERISK != t) && (ZWRITE_ALL != t))
		lvzwrite_block->mask |= 1 << sub_idx;
	if (ZWRITE_VAL != t)
		lvzwrite_block->fixed = FALSE;
	return;
}
