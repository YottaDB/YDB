/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"

boolean_t lcl_arg1_is_desc_of_arg2(lv_val *cur, lv_val *ref)
{
	lv_sbs_tbl	*tbl;
	lv_val		*lvp;

	if (cur == ref)
			return TRUE;
	tbl = cur->ptrs.val_ent.parent.sbs;
	while (MV_SYM != tbl->ident)
	{
		assert(tbl && MV_SBS == tbl->ident);
		lvp = tbl->lv;
		if (lvp == ref)
			return TRUE;
		tbl = lvp->ptrs.val_ent.parent.sbs;
	}
	return FALSE;
}
