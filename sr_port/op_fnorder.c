/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "numcmp.h"
#include "mvalconv.h"

GBLREF	mval		sbs_mval_int_ele;
GBLREF	collseq		*local_collseq;
GBLREF	boolean_t 	local_collseq_stdnull;
GBLREF	int         	lv_null_subs;
GBLREF	boolean_t	in_op_fnnext;

#define MINUS_ONE -MV_BIAS

void op_fnorder(lv_val *src, mval *key, mval *dst)
{
	int			cur_subscr;
	mval			tmp_sbs;
	int             	length;
	sbs_blk			*num, *str;
	boolean_t		found, is_neg;
	int4			i;
	lv_val			**lv;
	lv_sbs_tbl		*tbl;
	sbs_search_status	status;
	boolean_t		is_fnnext;

	is_fnnext = in_op_fnnext;
	in_op_fnnext = FALSE;
	found = FALSE;
	if (src)
	{
		if (tbl = src->ptrs.val_ent.children)
		{
			MV_FORCE_DEFINED(key);
			num = tbl->num;
			str = tbl->str;
			assert(tbl->ident == MV_SBS);
			if ((MV_IS_STRING(key) && key->str.len == 0) || (is_fnnext && MV_IS_INT(key) && key->m[1] == MINUS_ONE))
			{ /* With GT.M collation , if last subscript is null, $o returns the first subscript in that level */
				if (tbl->int_flag)
				{
					assert(num);
					for (i = 0, lv = &num->ptr.lv[0]; i < SBS_NUM_INT_ELE; i++, lv++)
					{
						if (*lv)
						{
							MV_FORCE_MVAL(dst,i);
							found = TRUE;
							break;
						}
					}
				} else if (num)
				{
					assert(num->cnt);
					MV_ASGN_FLT2MVAL((*dst),num->ptr.sbs_flt[0].flt);
					found = TRUE;
				}
			} else
			{
				if (MV_IS_CANONICAL(key))
				{
					MV_FORCE_NUM(key);
					if (tbl->int_flag)
					{
						assert(num);
						is_neg = (key->mvtype & MV_INT) ? key->m[1] < 0 : key->sgn;
						if (is_neg)
							i = 0;
						else
						{
							if (!is_fnnext && (1 == numcmp(key, (mval *)&sbs_mval_int_ele)))
								i = SBS_NUM_INT_ELE;
							else
							{
								i =  MV_FORCE_INT(key);
								i++;
							}
						}
						for (lv = &num->ptr.lv[i]; i < SBS_NUM_INT_ELE; i++, lv++)
						{
							if (*lv)
							{
								MV_FORCE_MVAL(dst,i);
								found = TRUE;
								break;
							}
						}
					} else if (num && lv_nxt_num_inx(num, key, &status))
					{
						MV_ASGN_FLT2MVAL((*dst),((sbs_flt_struct*)status.ptr)->flt);
						found = TRUE;
					}
				} else
				{
					if (local_collseq)
					{
						ALLOC_XFORM_BUFF(&key->str);
						tmp_sbs.mvtype = MV_STR;
						tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
						assert(NULL != lcl_coll_xform_buff);
						tmp_sbs.str.addr = lcl_coll_xform_buff;
						do_xform(local_collseq, XFORM, &key->str,
								&tmp_sbs.str, &length);
						tmp_sbs.str.len = length;
						s2pool(&(tmp_sbs.str));
						key = &tmp_sbs;
					}
					if (str && lv_nxt_str_inx(str, &key->str, &status))
					{
						dst->mvtype = MV_STR;
						dst->str = ((sbs_str_struct *)status.ptr)->str;
					} else
					{
						if (!is_fnnext)
						{
							dst->mvtype = MV_STR;
							dst->str.len = 0;
						} else
							MV_FORCE_MVAL(dst, -1);
					}
					found = TRUE;
				}
			}
			if (!found && str)
			{	/* We are here because
				 * a. key is "" and there is no numeric subscript, OR
				 * b. key is numeric and it is >= the largest numeric subscript at this level implying a switch from
				 *    numeric to string subscripts
				 * Either case, return the first string subscript. However, for STDNULLCOLL, skip to the next
				 * subscript should the first subscript be ""
				 */
				assert(str->cnt);
				dst->mvtype = MV_STR;
				dst->str = str->ptr.sbs_str[0].str;
				found = TRUE;
				if (local_collseq_stdnull && 0 == dst->str.len)
				{
					assert(LVNULLSUBS_OK == lv_null_subs);
					if (lv_nxt_str_inx(str, &dst->str, &status))
					{
						dst->str = ((sbs_str_struct*)status.ptr)->str;
					} else
						found = FALSE;
				}
			}
		}
	}
	if (!found)
	{
		if (!is_fnnext)
		{
			dst->mvtype = MV_STR;
			dst->str.len = 0;
		} else
			MV_FORCE_MVAL(dst, -1);
	} else if (dst->mvtype == MV_STR && local_collseq)
	{
		ALLOC_XFORM_BUFF(&dst->str);
		assert(NULL != lcl_coll_xform_buff);
		tmp_sbs.str.addr = lcl_coll_xform_buff;
		tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
		do_xform(local_collseq, XBACK,
				&dst->str, &tmp_sbs.str, &length);
		tmp_sbs.str.len = length;
		s2pool(&(tmp_sbs.str));
		dst->str = tmp_sbs.str;
	}
}
