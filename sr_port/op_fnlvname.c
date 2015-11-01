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

#include "gtm_string.h"

#include "hashdef.h"
#include "lv_val.h"
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;
GBLREF symval *curr_symval;

void op_fnlvname(mval *src,mval *dst)
{
	ht_entry *p, *min, *top;
	int n;
	mname b;
	unsigned char *c1, *c2, *ct;
	lv_sbs_tbl *tbl;
	lv_val *lv;

	if (stringpool.top - stringpool.free < sizeof(mname))
		stp_gcol(sizeof(mname));
	MV_FORCE_STR(src);
	n = src->str.len;
	if (n > sizeof(mname))
		n = sizeof(mname);
	for (c1 = (unsigned char *)src->str.addr, c2 = (unsigned char *)&b ; n > 0 ; n--)
		*c2++ = *c1++;
	while (c2 < (unsigned char *)& b.txt[sizeof(b)])
		*c2++ = 0;
	p = curr_symval->h_symtab.base;
	top = p + curr_symval->h_symtab.size;
	min = 0;
	for ( ; p < top ; p++)
	{
		if (p->nb.txt[0])
		{
			lv = (lv_val *)p->ptr;
			if (lv && (MV_DEFINED(&(lv->v)) || ((tbl = lv->ptrs.val_ent.children) && (tbl->num || tbl->str))))
			{
				n = memcmp(&b, &p->nb, sizeof(mname));
				if (n < 0)
				{
					if (!min)
						min = p;
					else
					{
						n = memcmp(&min->nb, &p->nb, sizeof(mname));
						if (n > 0)
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
		for (c1 = (unsigned char *)&min->nb, c2 = stringpool.free , ct = c1 + sizeof(mname) ; c1 < ct && *c1 ; )
			*c2++ = *c1++;
		dst->str.len = n = c2 - stringpool.free;
		dst->str.addr = (char *)stringpool.free;
		stringpool.free += n;
	}
	return;
}
