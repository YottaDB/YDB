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
#include "hashtab_mname.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "q_rtsib.h"
#include "mvalconv.h"

GBLREF boolean_t	local_collseq_stdnull;
LITREF mval		literal_null;

bool q_rtsib(lv_sbs_srch_hist *h1, mval *key)
{
	lv_sbs_srch_hist	*h0;
	lv_val			*parent;
	lv_sbs_tbl		*tbl;
	sbs_search_status	status;
	int			i;
	boolean_t		is_num;
	mstr			*keymstr;

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

/* Algorithm:
 *	!STDNULLCOLL:
 *		if (key is numeric)
 *		{
 *			if (numerics exist && next numeric available)
 *				return that;
 *			if (string subscripts exist)
 *				return first string subscript;
 *			return FALSE;
 *		}
 *		if (string subscripts exist && next string available)
 *			return that;
 *		return FALSE;
 *
 *	STDNULLCOLL:
 *		if (key is "")
 *		{
 *			if (numerics exist)
 *				return first numeric;
 *			if (string subscripts exist && next string available)
 *				return that;
 *			return FALSE;
 *		}
 *		if (key is numeric)
 *		{
 *			if (numerics exist && next numeric available)
 *				return that;
 *			if (string subscripts exist)
 *				return first string subscript;
 *			return FALSE;
 *		}
 *		if (string subscripts exist && next string available)
 *			return that;
 *		return FALSE;
 *
 *	For implementation, we'll combine stdnullcoll, key == "" and !stdnullcoll, numeric. Since the code for key is numeric and
 *	key is string are same regardless of stdnullcoll, we'll keep those cases under common code.
 */
	if (((FALSE != (is_num = MV_IS_CANONICAL(key))) || (local_collseq_stdnull && 0 == key->str.len)) && tbl->num)
	{ /* numeric subscript, or ""; numeric subscripts exist */
		assert(0 < tbl->num->cnt); /* must hold if there are numerics */
		MV_FORCE_NUM(key);
		if (tbl->int_flag)
		{ /* all numeric subscripts in range [0, SBS_NUM_INT_ELE) */
			i = (is_num ? MV_FORCE_INT(key) + 1 : 0); /* for stdnullcoll force search from 0 to locate first numeric */
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
				assert(is_num ||!local_collseq_stdnull); /* for stdnullcoll && key == "",
									we must have found numeric */
			}
		} else /* not all numeric subscripts are in [0, SBS_NUM_INT_ELE) range */
		{
			if (is_num)
			{ /* locate next in sequence */
				if (lv_nxt_num_inx (tbl->num, key, &status))
				{
					h1->type = SBS_BLK_TYPE_FLT;
					h1->addr.flt = (sbs_flt_struct *)status.ptr;
					return TRUE;
				} /* next not found in numerics, look for first string subscript if string subscripts exist */
			} else
			{ /* stdnullcoll and "" key, pick the first numeric subscript */
				h1->type = SBS_BLK_TYPE_FLT;
				h1->addr.flt = tbl->num->ptr.sbs_flt;
				return TRUE;
			}
		}
	}
	/* key is string, OR, highest numeric processed, now cross-over to string */
	if (tbl->str)
	{
		assert(0 < tbl->str->cnt);
		if (is_num)
		{	/* numeric to string cross-over, pick first string subscript except if it is "" and stdnullcoll is TRUE */
			if (0 != tbl->str->ptr.sbs_str[0].str.len || !local_collseq_stdnull)
			{
				h1->type = SBS_BLK_TYPE_STR;
				h1->addr.str = tbl->str->ptr.sbs_str;
				return TRUE;
			}
			/* stdnullcoll is TRUE and first string is "", force a search beyond the "" string */
			keymstr = &(tbl->str->ptr.sbs_str[0].str);
		} else
			keymstr = &key->str;
		if (lv_nxt_str_inx(tbl->str, keymstr, &status))
		{
			h1->type = SBS_BLK_TYPE_STR;
			h1->addr.str = (sbs_str_struct *)status.ptr;
			return TRUE;
		}
	}
	return FALSE;
}
