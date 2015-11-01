/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#define MINUS_ONE -MV_BIAS
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "mvalconv.h"
#include "underr.h"

GBLREF collseq		*local_collseq;

void op_fnnext(lv_val *src,mval *key,mval *dst)
{
	int     		cur_subscr;
	mval			tmp_sbs;
	int			length;
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
			if ((MV_IS_INT(key) && key->m[1] == MINUS_ONE) || (MV_IS_STRING(key) && key->str.len == 0))
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
							i =  MV_FORCE_INT(key);
							i++;
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
						ALLOC_XFORM_BUFF(&key->str);
						tmp_sbs.mvtype = MV_STR;
						tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
						assert(NULL != lcl_coll_xform_buff);
						tmp_sbs.str.addr = lcl_coll_xform_buff;
						do_xform(local_collseq->xform, &key->str, &tmp_sbs.str, &length);
						tmp_sbs.str.len = length;
						s2pool(&(tmp_sbs.str));
						key = &tmp_sbs;
					}
					if (str && lv_nxt_str_inx(str, &key->str, &status))
				 	{
						if (local_collseq)
						{
							ALLOC_XFORM_BUFF(&((sbs_str_struct *)status.ptr)->str);
							tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
							assert(NULL != lcl_coll_xform_buff);
							tmp_sbs.str.addr = lcl_coll_xform_buff;
							do_xform(local_collseq->xback,
								&((sbs_str_struct *) status.ptr)->str,
								&tmp_sbs.str,
								&length);
							tmp_sbs.str.len = length;
							s2pool(&(tmp_sbs.str));
							dst->str = tmp_sbs.str;
						}
						else
							dst->str = ((sbs_str_struct*)status.ptr)->str;
						dst->mvtype = MV_STR;
					}
					else
					{	MV_FORCE_MVAL(dst, -1);
					}
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
		MV_FORCE_MVAL(dst, -1);
}
