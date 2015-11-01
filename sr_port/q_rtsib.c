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
#include "q_rtsib.h"
#include "mvalconv.h"

bool q_rtsib (lv_sbs_srch_hist	*h1, mval *key)
{
	lv_sbs_srch_hist	*h0;
	lv_val			*parent;
	lv_sbs_tbl		*tbl;
	sbs_search_status	status;
	int			i;

	h0 = h1 - 1;
	switch (h0->type)
	{
	case SBS_BLK_TYPE_ROOT:
		parent = h0->addr.root;
		break;
	case SBS_BLK_TYPE_INT:
		parent = *h0->addr.intnum;
		break;
	case SBS_BLK_TYPE_FLT:
		parent = h0->addr.flt->lv;
		break;
	case SBS_BLK_TYPE_STR:
		parent = h0->addr.str->lv;
		break;
	default:
		GTMASSERT;
	}
	if (!(tbl = parent->ptrs.val_ent.children))
		return FALSE;

	if (MV_IS_CANONICAL(key))
	{	MV_FORCE_NUM(key);
		if (tbl->int_flag)
		{
			assert (tbl->num);
			i = MV_FORCE_INT(key) + 1;
			if (0 <= i && i < SBS_NUM_INT_ELE)
			{
				for ( ; i < SBS_NUM_INT_ELE && !tbl->num->ptr.lv[i]; i++)
					;
				if (i < SBS_NUM_INT_ELE)
				{
					h1->type = SBS_BLK_TYPE_INT;
					h1->addr.intnum = &tbl->num->ptr.lv[i];
					return TRUE;
				}
			}
		}
		else
		if (tbl->num && lv_nxt_num_inx (tbl->num, key, &status))
		{
			h1->type = SBS_BLK_TYPE_FLT;
			h1->addr.flt = (sbs_flt_struct *)status.ptr;
			return TRUE;
		}
	}
	else
	{
		if (tbl->str && lv_nxt_str_inx (tbl->str, &key->str, &status))
		{
			h1->type = SBS_BLK_TYPE_STR;
			h1->addr.str = (sbs_str_struct *)status.ptr;
			return TRUE;
		}
		else
			return FALSE;
	}
	if (tbl->str)
	{
		assert (tbl->str->cnt);
		h1->type = SBS_BLK_TYPE_STR;
		h1->addr.str = tbl->str->ptr.sbs_str;
		return TRUE;
	}
	return FALSE;
}
