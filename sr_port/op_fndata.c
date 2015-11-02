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

LITREF mval *fndata_table[2][2];

void op_fndata(lv_val *x, mval *y)
{
	int		r,s;

	r = s = 0;
	if (x)
	{
		if (LV_IS_VAL_DEFINED(x))
	  		r++;
		if (LV_HAS_CHILD(x))
			s++;
	}
	*y = *fndata_table[s][r];
}
