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

GBLREF lv_val *active_lv;
GBLREF short dollar_tlevel;

void	op_lvzwithdraw(lv_val *lv)
{
	lv_val		*tp_val;
 	lv_sbs_tbl	*tbl;

	active_lv = (lv_val *)0;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
						cleanup problems */
	if (lv)
	{
		if (0 < dollar_tlevel)
		{
			tp_val = lv;
		       	tbl = tp_val->ptrs.val_ent.parent.sbs;
			while (MV_SYM != tbl->ident)
			{
				tp_val = tbl->lv;
			       	tbl = tp_val->ptrs.val_ent.parent.sbs;
			}
			if (NULL != tp_val->tp_var)
				tp_var_clone(tp_val);/*	 clone the tree. */
		}
	       	lv->v.mvtype = 0;
       		tbl = lv->ptrs.val_ent.parent.sbs;
		if ((MV_SBS == tbl->ident) && (!lv->ptrs.val_ent.children))
		{
			assert(MV_SYM == tbl->sym->ident);
			lv->ptrs.free_ent.next_free = tbl->sym->lv_flist;
	 		tbl->sym->lv_flist = lv;
		 	lv_zap_sbs(tbl, lv);
		}
	}
}
