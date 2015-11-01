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

/*
 *--------------------------------------------------------------------------
 * Descriptipn:
 * 	Given a non-null lv_val *, this will format the entire local variable key
 * 	This will traverse the local variable tree towards root and
 *	do exhaustive search in the blocks to format all the subscripts.
 * Note:
 *	This is very inefficient way to format key. But given the
 *	local variable design there is no other simple solution
 *	to format a key from *lv_val.
 * Input:
 *	lvpin: Pointer to the last subscript's lv_val
 * 	buff: Buffer where key will be formatted
 * 	size: Size of buff
 * Return Value:
 *	End address upto which buffer was  used to format the key
 *	(Needed for the length calculation in caller)
 *--------------------------------------------------------------------------
 */

#include "mdef.h"

#include "subscript.h"
#include "hashdef.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "gtm_string.h"
#include "mvalconv.h"
#include "lvname_info.h"

void get_tbl_subs(lv_val *startlvp, lv_sbs_tbl *tbl, int type[], int index[], int cntfmt);

unsigned char	*format_key_lv_val(lv_val *lvpin, unsigned char *buff, int size)
{
	int		cnt;
	mval		tempmv;
	int		cntfmt = 0, type[MAX_LVSUBSCRIPTS], index[MAX_LVSUBSCRIPTS];
	lv_sbs_tbl      *tbl, *key_tbl[MAX_LVSUBSCRIPTS];
	lv_val		*startlvp;
	unsigned char	*endbuff;

	if (!lvpin)
		return buff;

	startlvp = lvpin;
	tbl = startlvp->ptrs.val_ent.parent.sbs;
	assert(tbl);
	while (MV_SYM != tbl->ident)
	{
		key_tbl[cntfmt] = tbl;
		get_tbl_subs(startlvp, tbl, type, index, cntfmt++);
		startlvp = tbl->lv;
		tbl = startlvp->ptrs.val_ent.parent.sbs;
	}
	assert(MV_SYM == tbl->ident);

	endbuff = format_lvname(startlvp, buff, size);
	size -= (endbuff - buff);
	buff = endbuff;

	if (cntfmt)
	{
		if (size < 1)
			return buff;
		*buff++ = '(';
		size--;
	}
	for(cnt = cntfmt - 1; cnt >= 0; cnt--)
	{
		switch(type[cnt])
		{
		case SBS_BLK_TYPE_INT:
			MV_FORCE_MVAL(&tempmv, index[cnt]);
			MV_FORCE_STR(&tempmv);
			break;
		case SBS_BLK_TYPE_FLT:
			MV_ASGN_FLT2MVAL(tempmv, key_tbl[cnt]->num->ptr.sbs_flt[index[cnt]].flt);
			MV_FORCE_STR(&tempmv);
			break;
		case SBS_BLK_TYPE_STR:
			tempmv.str = key_tbl[cnt]->str->ptr.sbs_str[index[cnt]].str;
			break;
		default:
			GTMASSERT;
			break;
		}
		if (size < tempmv.str.len)
		{
			/* copy as much space as we have */
			memcpy(buff, tempmv.str.addr, size);
			buff += size;
			return buff;
		}
		memcpy(buff, tempmv.str.addr, tempmv.str.len);
		size -= tempmv.str.len;
		buff += tempmv.str.len;
		if (cnt)
		{
			if (size < 1)
				return buff;
			*buff++ = ',';
			size--;
		}
	}
	if (cntfmt)
	{
		if (size < 1)
			return buff;
		*buff++ = ')';
		size--;
	}
	return buff;
}
void get_tbl_subs(lv_val *startlvp, lv_sbs_tbl *tbl, int type[], int index[], int cntfmt)
{
	int cnt;
	boolean_t found = FALSE;
	if (tbl->num)
	{
		if (tbl->int_flag)
		{
			for (cnt = 0; cnt < SBS_NUM_INT_ELE; cnt++)
			{
				if (tbl->num->ptr.lv[cnt] == startlvp)
				{
					type[cntfmt] = SBS_BLK_TYPE_INT;
					index[cntfmt] = cnt;
					found = TRUE;
					break;
				}
			}
		}
		else
		{
			for (cnt = 0; cnt < tbl->num->cnt; cnt++)
			{
				if (tbl->num->ptr.sbs_flt[cnt].lv == startlvp)
				{
					type[cntfmt] = SBS_BLK_TYPE_FLT;
					index[cntfmt] = cnt;
					found = TRUE;
					break;
				}
			}
		}
	}
	if (!found && tbl->str)
	{
		for (cnt = 0; cnt < tbl->str->cnt; cnt++)
		{
			if (tbl->str->ptr.sbs_str[cnt].lv == startlvp)
			{
				type[cntfmt] = SBS_BLK_TYPE_STR;
				index[cntfmt] = cnt;
				found = TRUE;
				break;
			}
		}
	}
	assert(found);
}
