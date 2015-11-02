/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF boolean_t	need_lvgcol;

LITREF mval 		*fndata_table[2][2];
LITREF mval 		*fnzdata_table[2][2];

void op_fnzdata(lv_val *srclv, mval *dst)
{
	lv_val 		*p;
	int 		isdefd, hasdesc;
	lv_sbs_tbl	*tbl;
	boolean_t	isalias;

	isdefd = hasdesc = 0;
	isalias = FALSE;
	if (srclv)
	{
		if (MV_DEFINED(&(srclv->v)))
	  		isdefd++;
       	       	if ((tbl = srclv->ptrs.val_ent.children) && (tbl->num || tbl->str))
		{
			assert(tbl->ident == MV_SBS);
			hasdesc++;
		}
		assert(srclv->ptrs.val_ent.parent.sym);
		if (MV_SYM == srclv->ptrs.val_ent.parent.sym->ident)
			/* This is an unsubscripted var -- check reference count */
			isalias = IS_ALIASLV(srclv);
		else
		{	/* Must be a subscript lv - check if a container */
			assert(MV_SBS == srclv->ptrs.val_ent.parent.sbs->ident);
			isalias = (0 != (MV_ALIASCONT & srclv->v.mvtype));
		}
	}
	*dst = isalias ? *fnzdata_table[hasdesc][isdefd] : *fndata_table[hasdesc][isdefd];
}
