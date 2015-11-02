/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "hashtab.h"
#include "hashtab_mname.h"      /* needed for lv_val.h */
#include "lv_val.h"
#include "stringpool.h"
#include "op.h"
#include "min_max.h"

GBLREF spdesc stringpool;
GBLREF symval *curr_symval;

void op_fnlvname(mval *src,mval *dst)
{
	ht_ent_mname	*p, *min, *top;
	int 		n;
	mident		name;
	lv_sbs_tbl 	*tbl;
	lv_val 		*lv;

	MV_FORCE_STR(src);
	name.addr = src->str.addr;
	name.len = (MAX_MIDENT_LEN > src->str.len) ? src->str.len : MAX_MIDENT_LEN;

	p = curr_symval->h_symtab.base;
	top = p + curr_symval->h_symtab.size;
	assert(top == curr_symval->h_symtab.top);
	min = 0;
	for ( ; p < top ; p++)
	{
		if (HTENT_VALID_MNAME(p, lv_val, lv))
		{
			if (lv && (MV_DEFINED(&(lv->v)) || ((tbl = lv->ptrs.val_ent.children) && (tbl->num || tbl->str))))
			{
				MIDENT_CMP(&name, &p->key.var_name, n);
				if (0 > n)
				{
					if (!min)
						min = p;
					else
					{
						MIDENT_CMP(&min->key.var_name, &p->key.var_name, n);
						if (0 < n)
							min = p;
					}
				}
			}
		}
	}
	dst->mvtype = MV_STR;
	if (!min)
		dst->str.len = 0;
	else
	{
		n = min->key.var_name.len;
		if (stringpool.top - stringpool.free < n)
		{
			dst->str.len = 0;	/* so stp_gcol ignores otherwise incompletely setup mval */
			stp_gcol(n);
		}
		memcpy(stringpool.free, min->key.var_name.addr, n);
		dst->str.len = n;
		dst->str.addr = (char *)stringpool.free;
		stringpool.free += n;
	}
	return;
}
