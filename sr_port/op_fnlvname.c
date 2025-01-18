/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "lv_val.h"
#include "stringpool.h"
#include "op.h"
#include "min_max.h"

GBLREF spdesc stringpool;
GBLREF symval *curr_symval;

void op_fnlvname(mval *src, boolean_t return_undef_aliases, symval *sym, mval *dst)
{
	ht_ent_mname	*p, *min, *top;
	int 		n;
	mident		name;
	lv_val 		*lv;

	MV_FORCE_STR(src);
	name.addr = src->str.addr;
	name.len = (MAX_MIDENT_LEN > src->str.len) ? src->str.len : MAX_MIDENT_LEN;

	if (NULL == sym)
		sym = curr_symval;
	p = sym->h_symtab.base;
	top = p + sym->h_symtab.size;
	assert(top == sym->h_symtab.top);
	min = 0;
	for ( ; p < top ; p++)
	{
		if (HTENT_VALID_MNAME(p, lv_val, lv) && '$' != *p->key.var_name.addr)	/* Avoid $ZWRTAC* variables in tree */
		{
			if (lv && (LV_IS_VAL_DEFINED(lv)
					|| LV_HAS_CHILD(lv)
					|| (return_undef_aliases && ((1 < lv->stats.trefcnt) || (0 < lv->stats.crefcnt)))))
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
	dst->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied by this to-be-overwritten mval */
	if (min)
	{
		n = min->key.var_name.len;
		ENSURE_STP_FREE_SPACE(n);
		memcpy(stringpool.free, min->key.var_name.addr, n);
		dst->str.len = n;
		dst->str.addr = (char *)stringpool.free;
		stringpool.free += n;
	} else
		dst->str.len = 0;
	dst->mvtype = MV_STR; /* initialize mvtype now that dst mval has been otherwise completely set up */
}
