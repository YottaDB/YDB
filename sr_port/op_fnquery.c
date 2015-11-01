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
#include "subscript.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "collseq.h"
#include "compiler.h"
#include "op.h"
#include "do_xform.h"
#include "q_rtsib.h"
#include "q_nxt_val_node.h"
#include "mvalconv.h"
#include <varargs.h>
#include "underr.h"

GBLREF spdesc		stringpool;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;
GBLREF collseq		*local_collseq;
GBLREF char		*lcl_coll_xform_buff;

void op_fnquery (va_alist)
va_dcl
{
	int             	length;
	mval            	tmp_sbs;
	va_list			var;
	mval			*dst, *varname, *v1, *v2;
	mval			*arg1, **argpp, *args[MAX_LVSUBSCRIPTS];
	mval			xform_args[MAX_LVSUBSCRIPTS];  /* for lclcol */
	mstr			format_out;
	lv_val			*v, *node, *parent;
	lv_sbs_srch_hist	*h1, *h2, history[MAX_LVSUBSCRIPTS];
	lv_sbs_tbl		*tbl;
	sbs_search_status	status;
	sbs_blk			*num, *str;
	int			i, j, sbscnt;
	bool			found, is_num;
	error_def		(ERR_STACKOFLOW);
	error_def		(ERR_STACKCRIT);

	VAR_START(var);
	sbscnt = va_arg(var, int4) - 3;
	dst = va_arg(var, mval *);
	varname = va_arg(var, mval *);
	v = va_arg(var, lv_val *);

	assert (v);
	if (!v->ptrs.val_ent.children)
	{
		dst->mvtype = MV_STR;
		dst->str.len = 0;
		return;
	}
	h1 = history;
	h1->type = SBS_BLK_TYPE_ROOT;
	h1->addr.root = v;
	h1++;
	if (local_collseq)
		tmp_sbs.mvtype = MV_STR;

	for (i = 0, node = v, argpp = &args[0];
	     i < sbscnt;
	     i++, argpp++, h1++)
	{
		*argpp = va_arg(var, mval *);

		if (tbl = node->ptrs.val_ent.children)
		{
			assert (tbl->ident == MV_SBS);
			MV_FORCE_DEFINED(*argpp);

			if (MV_IS_STRING(*argpp))
			{
				if (is_num = MV_IS_CANONICAL(*argpp))
					MV_FORCE_NUM(*argpp);
			}
			else
			{
				assert (MV_IS_NUMERIC(*argpp));
				assert (MV_IS_CANONICAL(*argpp));
				is_num = TRUE;
			}
			if (is_num)
			{
				if (tbl->int_flag)
				{
					assert (tbl->num);
					if (MV_IS_INT(*argpp) && (j = MV_FORCE_INT(*argpp)) < SBS_NUM_INT_ELE &&
					    tbl->num->ptr.lv[j])
					{
						h1->type = SBS_BLK_TYPE_INT;
						h1->addr.intnum = &tbl->num->ptr.lv[j];
						node = *h1->addr.intnum;
					}
					else
						break;
				}
				else
				{
					if (tbl->num && lv_get_num_inx (tbl->num, (*argpp), &status))
					{
						h1->type = SBS_BLK_TYPE_FLT;
						h1->addr.flt = (sbs_flt_struct *)status.ptr;
						node = h1->addr.flt->lv;
					}
					else
						break;
				}
			}
			else	/* is_string */
			{
				if (local_collseq)
				{
					/* D9607-258 changed xback to xform and added
					   xform_args[] to hold mval pointing to
					   xform'd subscript which is in pool space.
					   tmp_sbs (which is really a mval) was being
					   overwritten if >1 alpha subscripts */
					assert(NULL != lcl_coll_xform_buff);
					tmp_sbs.str.addr = lcl_coll_xform_buff;
					tmp_sbs.str.len = MAX_LCL_COLL_XFORM_BUFSIZ;
					/* KMK subscript index is i+1 */
					do_xform(local_collseq->xform /* was xback */,
						 &(*argpp)->str, &tmp_sbs.str,
						 &length);
					tmp_sbs.str.len = length;
					s2pool(&(tmp_sbs.str));
					xform_args[i] = tmp_sbs;
					*argpp = &xform_args[i];
				}
				if (tbl->str && lv_get_str_inx (tbl->str, &(*argpp)->str, &status))
				{
					h1->type = SBS_BLK_TYPE_STR;
					h1->addr.str = (sbs_str_struct *)status.ptr;
					node = h1->addr.str->lv;
				}
				else
					break;
			}
		}
		else
			break;
	}
	found = FALSE;
	if (i == sbscnt)
	{
		switch ((h1 - 1)->type)
		{
			case SBS_BLK_TYPE_ROOT:
				parent = (h1 - 1)->addr.root;
				break;
			case SBS_BLK_TYPE_INT:
				parent = *(h1 - 1)->addr.intnum;
				break;
			case SBS_BLK_TYPE_FLT:
				parent = (h1 - 1)->addr.flt->lv;
				break;
			case SBS_BLK_TYPE_STR:
				parent = (h1 - 1)->addr.str->lv;
				break;
			default:
				GTMASSERT;
		}
		assert (parent);
		if (tbl = parent->ptrs.val_ent.children)
		{
			found = TRUE;
			if (num = tbl->num)
			{
				assert (num->cnt);
				if (tbl->int_flag)
				{
					for (i = 0; !num->ptr.lv[i]; i++)
						;
					assert (i < SBS_NUM_INT_ELE);
					h1->type = SBS_BLK_TYPE_INT;
					h1->addr.intnum = &num->ptr.lv[i];
				}
				else
				{
					h1->type = SBS_BLK_TYPE_FLT;
					h1->addr.flt = num->ptr.sbs_flt;
				}
			}
			else
			{
				str = tbl->str;
				assert (str);
				assert (str->cnt);
				h1->type = SBS_BLK_TYPE_STR;
				h1->addr.str = str->ptr.sbs_str;
			}
		}
		else
		{
			--h1;
			--argpp;
		}
	}

	if (!found)
		for (;;)
		{
			if (h1 == history)
			{
				dst->mvtype = MV_STR;
				dst->str.len = 0;
				return;
			}
			if (q_rtsib (h1, *argpp))
				break;
			else
			{
				--h1;
				--argpp;
			}
		}
	q_nxt_val_node (&h1);

	/* format the output string */
	if (stringpool.top - stringpool.free < varname->str.len + 1)
		stp_gcol (varname->str.len + 1);
	PUSH_MV_STENT(MVST_MVAL);
	v1 = &mv_chain->mv_st_cont.mvs_mval;
	PUSH_MV_STENT(MVST_MVAL);
	v2 = &mv_chain->mv_st_cont.mvs_mval;
	v1->mvtype = MV_STR;
	v1->str.len = 0;
	v1->str.addr = (char *)stringpool.free;
	memcpy (stringpool.free, varname->str.addr, varname->str.len);
	stringpool.free += varname->str.len;
	*stringpool.free++ = '(';
	for (h2 = &history[1]; h2 <= h1; h2++)
	{
		switch (h2->type)
		{
			case SBS_BLK_TYPE_INT:
				switch ((h2 - 1)->type)
				{
					case SBS_BLK_TYPE_ROOT:
						parent = (h2 - 1)->addr.root;
						break;
					case SBS_BLK_TYPE_INT:
						parent = *(h2 - 1)->addr.intnum;
						break;
					case SBS_BLK_TYPE_FLT:
						parent = (h2 - 1)->addr.flt->lv;
						break;
					case SBS_BLK_TYPE_STR:
						parent = (h2 - 1)->addr.str->lv;
						break;
					default:
						GTMASSERT;
				}
				assert (parent->ptrs.val_ent.children);
				assert (parent->ptrs.val_ent.children->num);
				assert (parent->ptrs.val_ent.children->int_flag);
				assert ((unsigned char *) parent->ptrs.val_ent.children->num->ptr.lv <=
					(unsigned char *) h2->addr.intnum);
				assert ((unsigned char *) h2->addr.intnum <
					(unsigned char *) parent->ptrs.val_ent.children->num->ptr.lv + sizeof (sbs_blk));
				if (stringpool.top - stringpool.free < MAX_NUM_SIZE)
				{
					v1->str.len = (char *)stringpool.free - v1->str.addr;
					stp_gcol (MAX_NUM_SIZE);
					assert (v1->str.len == (char *)stringpool.free - v1->str.addr);
				}
				j = (int)(h2->addr.intnum - parent->ptrs.val_ent.children->num->ptr.lv);
				MV_FORCE_MVAL(v2, j);
				n2s(v2);
				break;
			case SBS_BLK_TYPE_FLT:
				if (stringpool.top - stringpool.free < MAX_NUM_SIZE)
				{
					v1->str.len = (char *)stringpool.free - v1->str.addr;
					stp_gcol (MAX_NUM_SIZE);
					assert (v1->str.len == (char *)stringpool.free - v1->str.addr);
				}
				MV_ASGN_FLT2MVAL(*v2, h2->addr.flt->flt);
				n2s(v2);
				break;
			case SBS_BLK_TYPE_STR:
				v1->str.len = (char *)stringpool.free - v1->str.addr;
				v2->mvtype = MV_STR;
				if (local_collseq)
				{
					assert(NULL != lcl_coll_xform_buff);
					tmp_sbs.str.addr = lcl_coll_xform_buff;
					tmp_sbs.str.len = MAX_LCL_COLL_XFORM_BUFSIZ;
					do_xform(local_collseq->xback, &h2->addr.str->str, &tmp_sbs.str, &length);
					tmp_sbs.str.len = length;
					v2->str = tmp_sbs.str;
				}
				else
					v2->str = h2->addr.str->str;
				mval_lex(v2, &format_out);
				if (format_out.addr != (char *)stringpool.free)
				{	/* We must put the string on the string pool ourself - mval_lex didn't do it
					   because v2 is a canonical numeric string. It is canonical but has too many
					   digits to be treated as a number. It must be output as a quoted string. */
					if (stringpool.top - stringpool.free < v2->str.len + 2)
					{
						v1->str.len = (char *)stringpool.free - v1->str.addr;
						stp_gcol (v2->str.len + 2);
						assert (v1->str.len == (char *)stringpool.free - v1->str.addr);
					}
					*stringpool.free++ = '\"';
					memcpy(stringpool.free, v2->str.addr, v2->str.len);
					stringpool.free += v2->str.len;
					*stringpool.free++ = '\"';
				} else
					stringpool.free += format_out.len;
				break;
			default:
				GTMASSERT;
		}
		if (stringpool.top == stringpool.free)
		{
			v1->str.len = (char *)stringpool.free - v1->str.addr;
			stp_gcol (1);
			assert (v1->str.len == (char *) stringpool.free - v1->str.addr);
		}
		*stringpool.free++ = ( h2 < h1 ? ',' : ')' ) ;
	}
	dst->mvtype = MV_STR;
	dst->str.len = (char *)stringpool.free - v1->str.addr;
	dst->str.addr = v1->str.addr;
	POP_MV_STENT(); /* v2 */
	POP_MV_STENT(); /* v1 */
}
