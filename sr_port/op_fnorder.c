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
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "numcmp.h"
#include "mvalconv.h"
#include "underr.h"

LITREF	mval SBS_MVAL_INT_ELE;

GBLREF collseq		*local_collseq;
GBLREF char		*lcl_coll_xform_buff;

void op_fnorder(lv_val *src,mval *key,mval *dst)
{
	int			cur_subscr;
	mval			tmp_sbs;
	int             	length;
	sbs_blk			*num, *str;
	bool			found, is_neg;
	int4			i;
	lv_val			**lv;
	lv_sbs_tbl		*tbl;
	sbs_search_status	status;

	found = FALSE;
	if (src)
	{	if (tbl = src->ptrs.val_ent.children)
		{	MV_FORCE_DEFINED(key);
			num = tbl->num;
			str = tbl->str;
			assert(tbl->ident == MV_SBS);
			if (MV_IS_STRING(key) && key->str.len == 0)
			{	if (tbl->int_flag)
				{	assert(num);
					for (i = 0, lv = &num->ptr.lv[0]; i < SBS_NUM_INT_ELE; i++, lv++)
					{	if (*lv)
						{	MV_FORCE_MVAL(dst,i);
							found = TRUE;
							break;
						}
					}
				}
				else if (num)
				{	assert(num->cnt);
					MV_ASGN_FLT2MVAL((*dst),num->ptr.sbs_flt[0].flt);
					found = TRUE;
				}
			}
			else
			{
				if (MV_IS_CANONICAL(key))
				{	MV_FORCE_NUM(key);
					if (tbl->int_flag)
					{	assert(num);
						is_neg = (key->mvtype & MV_INT) ? key->m[1] < 0 : key->sgn;
						if (is_neg)
							i = 0;
						else
						{
							if (numcmp(key,&SBS_MVAL_INT_ELE)==1)
								i = SBS_NUM_INT_ELE;
							   else
								{i =  MV_FORCE_INT(key);
								 i++;
								}
						}
						for (lv = &num->ptr.lv[i]; i < SBS_NUM_INT_ELE; i++, lv++)
						{	if (*lv)
							{	MV_FORCE_MVAL(dst,i);
								found = TRUE;
								break;
							}
						}
					}
					else if (num && lv_nxt_num_inx(num, key, &status))
					{
						MV_ASGN_FLT2MVAL((*dst),((sbs_flt_struct*)status.ptr)->flt);
						found = TRUE;
					}
				}
				else
				{
					if (local_collseq)
					{
						tmp_sbs.mvtype = MV_STR;
						tmp_sbs.str.len = MAX_LCL_COLL_XFORM_BUFSIZ;
						assert(NULL != lcl_coll_xform_buff);
						tmp_sbs.str.addr = lcl_coll_xform_buff;
						do_xform(local_collseq->xform, &key->str, &tmp_sbs.str, &length);
						tmp_sbs.str.len = length;
						s2pool(&(tmp_sbs.str));
						key = &tmp_sbs;
					}
					if (str && lv_nxt_str_inx(str, &key->str, &status))
				 	{
						dst->str = ((sbs_str_struct*)status.ptr)->str;
					}
					else
						dst->str.len = 0;
					dst->mvtype = MV_STR;
					found = TRUE;
				}
			}
			if (!found && str)
			{	assert(str->cnt);
				dst->mvtype = MV_STR;
				dst->str = str->ptr.sbs_str[0].str;
				found = TRUE;
			}
		}
	}
	if (!found)
	{	dst->mvtype = MV_STR;
		dst->str.len = 0;
	} else if (dst->mvtype == MV_STR && local_collseq)
	{
		assert(NULL != lcl_coll_xform_buff);
		tmp_sbs.str.addr = lcl_coll_xform_buff;
		tmp_sbs.str.len = MAX_LCL_COLL_XFORM_BUFSIZ;
		do_xform(local_collseq->xback,
			&dst->str,
			&tmp_sbs.str,
			&length);
		tmp_sbs.str.len = length;
		s2pool(&(tmp_sbs.str));
		dst->str = tmp_sbs.str;
	}
}
