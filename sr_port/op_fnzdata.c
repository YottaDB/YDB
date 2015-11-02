/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
	int 		isdefd, hasdesc;
	boolean_t	isalias, is_base_var;

	isdefd = hasdesc = 0;
	isalias = FALSE;
	if (srclv)
	{
		if (LV_IS_VAL_DEFINED((srclv)))
	  		isdefd++;
		is_base_var = LV_IS_BASE_VAR(srclv);
       	       	if (LV_HAS_CHILD(srclv))
			hasdesc++;
		if (is_base_var)
		{	/* This is an unsubscripted var -- check reference count */
			isalias = IS_ALIASLV(srclv);
		} else
		{	/* Must be a subscript lv - check if a container */
			/* ensure "srclv" is of type "lvTreeNode *" or "lvTreeNodeNum *" */
			assert(IS_LVAVLTREENODE(srclv));
			isalias = (0 != (MV_ALIASCONT & srclv->v.mvtype));
		}
	}
	*dst = isalias ? *fnzdata_table[hasdesc][isdefd] : *fndata_table[hasdesc][isdefd];
}
