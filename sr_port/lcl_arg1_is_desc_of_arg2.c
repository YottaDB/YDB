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

#include "lv_val.h"

boolean_t lcl_arg1_is_desc_of_arg2(lv_val *cur, lv_val *ref)
{
	lv_val		*lv;
	lvTree		*lvt;

	if (cur == ref)
		return TRUE;
	lv = cur;
	while (!LV_IS_BASE_VAR(lv))
	{
		lvt = LV_GET_PARENT_TREE(lv);
		lv = (lv_val *)LVT_PARENT(lvt);
		if (lv == ref)
			return TRUE;
	}
	return FALSE;
}
