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
#include "sbs_blk.h"
#include "q_nxt_val_node.h"

void q_nxt_val_node (lv_sbs_srch_hist **hpp)		/* guard the size of history? */
{
	lv_sbs_srch_hist	*h1;
 	lv_sbs_tbl		*tbl;
	int			i;
	sbs_blk			*num, *str;

	h1 = *hpp;
	assert (h1);
	assert ((h1->type == SBS_BLK_TYPE_INT && *h1->addr.intnum) ||
		(h1->type == SBS_BLK_TYPE_FLT && h1->addr.flt->lv) ||
		(h1->type == SBS_BLK_TYPE_STR && h1->addr.str->lv));
	for (;;)
	{
		switch (h1->type)
		{
		case SBS_BLK_TYPE_INT:
			if (MV_DEFINED(&(*h1->addr.intnum)->v))
				return;
			else
				tbl = (*h1->addr.intnum)->ptrs.val_ent.children;
			break;
		case SBS_BLK_TYPE_FLT:
			if (MV_DEFINED(&(h1->addr.flt->lv->v)))
				return;
			else
				tbl = h1->addr.flt->lv->ptrs.val_ent.children;
			break;
		case SBS_BLK_TYPE_STR:
			if (MV_DEFINED(&(h1->addr.str->lv->v)))
				return;
			else
				tbl = h1->addr.str->lv->ptrs.val_ent.children;
			break;
		default:
			GTMASSERT;
		}
		(*hpp)++;
		h1 = *hpp;
		assert (tbl);
		if (tbl->num)
		{
			assert (tbl->num->cnt);
			if (tbl->int_flag)
			{
				for (i = 0; !tbl->num->ptr.lv[i]; i++)
					;
				assert (i < SBS_NUM_INT_ELE);
				h1->type = SBS_BLK_TYPE_INT;
				h1->addr.intnum = &tbl->num->ptr.lv[i];
			}
			else
			{
				h1->type = SBS_BLK_TYPE_FLT;
				h1->addr.flt = tbl->num->ptr.sbs_flt;
			}
		}
		else
		{
			assert (tbl->str);
			assert (tbl->str->cnt);
			h1->type = SBS_BLK_TYPE_STR;
			h1->addr.str = tbl->str->ptr.sbs_str;
		}
	}
}
