/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

int is_equ(mval *u,mval *v)
{
	int		land, lor, utyp, vtyp;

	utyp = u->mvtype;
	vtyp = v->mvtype;
	land = utyp & vtyp;
	lor = utyp | vtyp;
	if ((land & MV_NM) != 0 && (lor & MV_NUM_APPROX) == 0)
	{	/* at this point, the mval's are both exact numbers, we can do a numeric comparison */
		if (land & MV_INT)	/* If they are both integers, compare only the relevant cells */
			return (u->m[1] == v->m[1]);
		if (lor & MV_INT)
		{	/* If one is an integer and the other is not, the two values cannot be equal */
			assert((vtyp & MV_INT) ? (u->m[1] || u->m[0]) : (v->m[1] || v->m[0]));	/* zero value must be INT */
			return 0;
		}
		/* They are both decimal floating numbers, do a full comparison */
		return ((((mval_b *)u)->sgne == ((mval_b *)v)->sgne) && (u->m[1] == v->m[1]) && (u->m[0]==v->m[0]));
	}
	/* At least one of the numbers is not in numeric form or is not a cannoical number, do a string compare */
	MV_FORCE_STR(u);
	MV_FORCE_STR(v);
	if ((u->str.len != v->str.len)
			|| (u->str.len && (u->str.addr != v->str.addr) && memcmp(u->str.addr, v->str.addr, u->str.len)))
		return 0;
	else
		return 1;
}
