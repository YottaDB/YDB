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

lv_val *lv_getslot(symval *sym)
{
	lv_blk *p,*q;
	int n;
	lv_val *x;

	n = 0;
	if (sym->lv_flist)
	{
		x = sym->lv_flist;
		sym->lv_flist = x->ptrs.free_ent.next_free;
	} else
	{
		for (p = &sym->first_block ; ; p = p->next)
		{
			if (!p)
			{
				p = lv_newblock(malloc(sizeof(lv_blk)), &sym->first_block, n > 64 ? 128 : n * 2);
				p->next = sym->first_block.next;
				sym->first_block.next = p;
			}
			if (n < p->lv_top - p->lv_base)
				n = p->lv_top - p->lv_base;
			if (p->lv_free < p->lv_top)
			{
				x = p->lv_free++;
				break;
			}
		}
	}
	return x;
}
