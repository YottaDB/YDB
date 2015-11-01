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
#include "hashdef.h"
#include "lv_val.h"

LITREF mval *fndata_table[2][2];

void op_fndata(x,y)
lv_val *x;
mval *y;
{
	lv_val *p;
	int r,s;
	lv_sbs_tbl	*tbl;

	r = s = 0;
	if (x)
	{
		if (MV_DEFINED(&(x->v)))
	  	{	r++;
		}
       	       	if ((tbl = x->ptrs.val_ent.children) && (tbl->num || tbl->str))
		{	assert(tbl->ident == MV_SBS);
			s++;
		}
	}
	*y = *fndata_table[s][r];
}
