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

/*
 *--------------------------------------------------------------------------
 * Descriptipn:
 * 	Given a non-null lv_val *, this will format the subscripted local variable key
 *	starting from the base variable lv_val *.
 * Input:
 *	lvpin: Pointer to the last subscript's lv_val
 * 	buff : Buffer where key will be formatted
 * 	size : Size of buff
 * Return Value:
 *	End address upto which buffer was used to format the key
 *	(Needed for length calculation in caller)
 *--------------------------------------------------------------------------
 */

#include "mdef.h"

#include "lv_val.h"
#include "gtm_string.h"
#include "mvalconv.h"
#include "promodemo.h"	/* for "demote" prototype used in LV_NODE_GET_KEY */

unsigned char	*format_key_lv_val(lv_val *lvpin, unsigned char *buff, int size)
{
	boolean_t	is_base_var;
	int		cnt, cntfmt;
	lv_val		*lv, *base_lv;
	mval		tempmv;
	lvTree		*lvt;
	lvTreeNode	*node, *nodep[MAX_LVSUBSCRIPTS];
	unsigned char	*endbuff;

	if (NULL == lvpin)
		return buff;
	lv = lvpin;
	is_base_var = LV_IS_BASE_VAR(lv);
	base_lv = !is_base_var ? LV_GET_BASE_VAR(lv) : lv;
	cntfmt = 0;
	while (lv != base_lv)
	{
		assert(!LV_IS_BASE_VAR(lv));
		nodep[cntfmt++] = (lvTreeNode *)lv;
		lvt = LV_GET_PARENT_TREE(lv);
		assert(NULL != lvt);
		assert(lvt->base_lv == base_lv);
		lv = (lv_val *)LVT_PARENT(lvt);
		assert(NULL != lv);
	}
	endbuff = format_lvname(base_lv, buff, size);
	size -= (int)(endbuff - buff);
	buff = endbuff;
	if (cntfmt)
	{
		if (size < 1)
			return buff;
		*buff++ = '(';
		size--;
	}
	for (cnt = cntfmt - 1; cnt >= 0; cnt--)
	{
		node = nodep[cnt];
		LV_NODE_GET_KEY(node, &tempmv); /* Get node key into "tempmv" depending on the structure type of "node" */
		MV_FORCE_STRD(&tempmv);
		if (size < tempmv.str.len)
		{	/* copy as much space as we have */
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
