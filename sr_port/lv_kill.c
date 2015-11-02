/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF lv_val	*active_lv;
GBLREF short	dollar_tlevel;

void	lv_kill(lv_val *lv, boolean_t dotpsave)
{
	lv_val		*tp_val;
 	lv_sbs_tbl	*tbl, *tmpsbs;

	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
	if (lv)
	{
		if (dotpsave && dollar_tlevel)
		{
			tp_val = lv;
		       	tbl = tp_val->ptrs.val_ent.parent.sbs;
			while (MV_SYM != tbl->ident)
			{
				tp_val = tbl->lv;
			       	tbl = tp_val->ptrs.val_ent.parent.sbs;
			}
			if (NULL != tp_val->tp_var && !tp_val->tp_var->var_cloned)
				TP_VAR_CLONE(tp_val);	/* clone the tree. */
		}
		if (tmpsbs = lv->ptrs.val_ent.children)	/* Note assignment */
		{
			lv->ptrs.val_ent.children = NULL;
		      	lv_killarray(tmpsbs, dotpsave);
		}
	       	tbl = lv->ptrs.val_ent.parent.sbs;
		assert(tbl);
		DECR_AC_REF(lv, dotpsave);		/* Decrement alias container refs and cleanup if necessary */
		if (MV_SBS == tbl->ident)
		{	/* Subscripted node */
			assert(MV_SYM == tbl->sym->ident);
			LV_FLIST_ENQUEUE(&tbl->sym->lv_flist, lv);	/* Note: clears mvtype/ident */
		 	lv_zap_sbs(tbl, lv);
		} else
	       	{	/* Base node */
			assert(MV_SYM == tbl->ident);
			lv->v.mvtype = 0;
		}
	}
}
