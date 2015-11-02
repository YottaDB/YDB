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

#include <stdarg.h>
#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "subscript.h"
#include "stringpool.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "collseq.h"
#include "compiler.h"
#include "op.h"
#include "do_xform.h"
#include "q_rtsib.h"
#include "q_nxt_val_node.h"
#include "mvalconv.h"
#include "numcmp.h"

GBLREF spdesc		stringpool;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;
GBLREF collseq		*local_collseq;
GBLREF mval		last_fnquery_return_varname;
GBLREF mval		last_fnquery_return_sub[MAX_LVSUBSCRIPTS];
GBLREF int		last_fnquery_return_subcnt;
GBLREF boolean_t 	local_collseq_stdnull;
GBLREF int		lv_null_subs;
GBLREF int4		lv_sbs_blk_size;

void op_fnquery (UNIX_ONLY_COMMA(int sbscnt) mval *dst, ...)
{
	int			length;
	mval		 	tmp_sbs;
	va_list			var;
	mval			*varname, *v1, *v2;
	mval			*arg1, **argpp, *args[MAX_LVSUBSCRIPTS], **argpp2, *lfrsbs, *argp2;
	mval			xform_args[MAX_LVSUBSCRIPTS];	/* for lclcol */
	mstr			format_out;
	lv_val			*v, *node, *parent;
	lv_sbs_srch_hist	*h1, *h2, history[MAX_LVSUBSCRIPTS];
	lv_sbs_tbl		*tbl;
	sbs_search_status	status;
	sbs_blk			*num, *str;
	int			i, j;
	VMS_ONLY(int		sbscnt;)
	boolean_t		found, is_num, last_sub_null, nullify_term;

	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);
	error_def(ERR_LVNULLSUBS);

	VAR_START(var, dst);
	VMS_ONLY(va_count(sbscnt);)
	assert(3 <= sbscnt);
	sbscnt -= 3;
	varname = va_arg(var, mval *);
	v = va_arg(var, lv_val *);
	assert(v);
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
			assert(MV_SBS == tbl->ident);
			MV_FORCE_DEFINED(*argpp);
			if (MV_IS_STRING(*argpp))
			{
				if ((0 == (*argpp)->str.len) && (i + 1 != sbscnt) && (LVNULLSUBS_NEVER == lv_null_subs))
				{	/* This is not the last subscript, we don't allow nulls subs and it was null */
					va_end(var);
					rts_error(VARLSTCNT(1) ERR_LVNULLSUBS);
				}
				if (is_num = MV_IS_CANONICAL(*argpp))
				{
					MV_FORCE_NUM(*argpp);
				} else if ((i + 1 == sbscnt) && (0 == (*argpp)->str.len) && !local_collseq_stdnull)
				{ 	/* The last search argument is a null string. For this situation, there is the possibility
					   of a syntax collision if (1) the user had (for example) specified $Q(a(1,3,"") to get
					   the first a(1,3,x) node or (2) this is the "next" element that was legitimately returned
					   by $Query on the last call. If the element a(1,3,"") actually exists, the code whereby
					   we simply strip off the last null search argument and continue will just find this
					   element again. Because of this we save the mvals of the subscrips of the last $QUERY
					   result if the result had a null last subscript. In that case, we will compare the input
					   variable and subscripts with the last returned value. If they are identical, we will
					   bypass the elimination of the trailing null subscript. This will allow the successor
					   element to be found. Note that this is a single value that is kept for the entire
					   process. Any intervening $QUery calls for other local variables will reset the check.
					   SE 2/2004 (D9D08-002352).
					*/
					nullify_term = TRUE;
					if (last_fnquery_return_varname.str.len
				    		&& last_fnquery_return_varname.str.len == varname->str.len
				    		&& 0 == memcmp(last_fnquery_return_varname.str.addr, varname->str.addr,
						varname->str.len)
				    		&& sbscnt == last_fnquery_return_subcnt)
					{	/* We have an equalvalent varname and same number subscripts */
						for (j = 0, argpp2 = &args[0], lfrsbs = &last_fnquery_return_sub[0];
					     		j < i; j++, argpp2++, lfrsbs++)
						{	/* For each subscript prior to the trailing null subscript */
							argp2 = *argpp2;
							if (MV_IS_NUMERIC(argp2) && MV_IS_NUMERIC(lfrsbs))
							{	/* Have numeric subscripts */
								if (0 != numcmp(argp2, lfrsbs))
									break;	/* This subscript isn't the same */
							} else if (MV_IS_STRING(argp2) && MV_IS_STRING(lfrsbs))
							{	/* Should be string only in order to compare */
								if ((argp2)->str.len == lfrsbs->str.len
							    		&& 0 != memcmp((argp2)->str.addr, lfrsbs->str.addr,
									   lfrsbs->str.len))
									break;	/* This subscript isn't the same */
							} else
								break;		/* This subscript isn't even close.. */
						}
						if (j == i)
							nullify_term = FALSE;/* We made it through the loop unscathed !! */
					}
					if (nullify_term)
					{
						i++;
						break;
					}
				}
			} else /* not string */
			{
				assert(MV_IS_NUMERIC(*argpp));
				assert(MV_IS_CANONICAL(*argpp));
				is_num = TRUE;
			}
			if (is_num)
			{
				if (tbl->int_flag)
				{
					assert(tbl->num);
					if (MV_IS_INT(*argpp) && (j = MV_FORCE_INT(*argpp)) < SBS_NUM_INT_ELE &&
						tbl->num->ptr.lv[j])
					{
						h1->type = SBS_BLK_TYPE_INT;
						h1->addr.intnum = &tbl->num->ptr.lv[j];
						node = *h1->addr.intnum;
					} else
						break;
				} else
				{
					if (tbl->num && lv_get_num_inx (tbl->num, (*argpp), &status))
					{
						h1->type = SBS_BLK_TYPE_FLT;
						h1->addr.flt = (sbs_flt_struct *)status.ptr;
						node = h1->addr.flt->lv;
					} else
						break;
				}
			} else
			{	/* is_string */
				if (local_collseq)
				{
					ALLOC_XFORM_BUFF(&(*argpp)->str);
					/* D9607-258 changed xback to xform and added
					   xform_args[] to hold mval pointing to
					   xform'd subscript which is in pool space.
					   tmp_sbs (which is really a mval) was being
					   overwritten if >1 alpha subscripts */
					assert(NULL != lcl_coll_xform_buff);
					tmp_sbs.str.addr = lcl_coll_xform_buff;
					tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
					/* KMK subscript index is i+1 */
					do_xform(local_collseq, XFORM /* was xback */,
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
				} else
					break;
			}
		} else
			break;
	}
	va_end(var);
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
		assert(parent);
		if (tbl = parent->ptrs.val_ent.children)	/* Note assignment! */
		{
			found = TRUE;
			if ((!local_collseq_stdnull || !tbl->str || 0 != tbl->str->ptr.sbs_str[0].str.len) && (num = tbl->num))
			{										 /*CAUTION assignment */
				assert(num->cnt);
				if (tbl->int_flag)
				{
					for (i = 0; !num->ptr.lv[i]; i++)
						;
					assert(i < SBS_NUM_INT_ELE);
					h1->type = SBS_BLK_TYPE_INT;
					h1->addr.intnum = &num->ptr.lv[i];
				} else
				{
					h1->type = SBS_BLK_TYPE_FLT;
					h1->addr.flt = num->ptr.sbs_flt;
				}
			} else
			{
				str = tbl->str;
				assert(str);
				assert(str->cnt);
				h1->type = SBS_BLK_TYPE_STR;
				h1->addr.str = str->ptr.sbs_str;
			}
		} else
		{
			--h1;
			--argpp;
		}
	}
	last_fnquery_return_subcnt = 0;		/* Saved last query result is irrelevant now */
	last_fnquery_return_varname.mvtype = MV_STR;
	last_fnquery_return_varname.str.len = 0;
	if (!found)
	{
		for (;;)
		{
			if (h1 == history)
			{
				dst->mvtype = MV_STR;
				dst->str.len = 0;
				return;
			}
			if (q_rtsib(h1, *argpp))
				break;
			else
			{
				--h1;
				--argpp;
			}
		}
	}
	q_nxt_val_node(&h1);

	/* Before we start formatting for output, decide whether we will be saving mvals of our subscripts
	   as we format. We only do this if the last subscript is a null. Bypassing it otherwise is a time
	   saver..
	   Note that with standard collation last subscript null has no special meaning
	*/
	last_sub_null = (SBS_BLK_TYPE_STR == h1->type &&  0 == h1->addr.str->str.len && !local_collseq_stdnull);

	/* format the output string */
	ENSURE_STP_FREE_SPACE(varname->str.len + 1);
	PUSH_MV_STENT(MVST_MVAL);
	v1 = &mv_chain->mv_st_cont.mvs_mval;
	v1->mvtype = MV_STR;
	v1->str.len = 0;
	v1->str.addr = (char *)stringpool.free;
	PUSH_MV_STENT(MVST_MVAL);
	v2 = &mv_chain->mv_st_cont.mvs_mval;
	v2->mvtype = 0;	/* initialize it to 0 to avoid stp_gcol from getting confused if it gets invoked before v2 has been
			 * completely setup. */
	memcpy(stringpool.free, varname->str.addr, varname->str.len);
	if (last_sub_null)
	{
		last_fnquery_return_varname.str.addr = (char *)stringpool.free;
		last_fnquery_return_varname.str.len += varname->str.len;
	}
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
				assert(parent->ptrs.val_ent.children);
				assert(parent->ptrs.val_ent.children->num);
				assert(parent->ptrs.val_ent.children->int_flag);
				assert((unsigned char *)parent->ptrs.val_ent.children->num->ptr.lv <=
					(unsigned char *)h2->addr.intnum);
				assert((unsigned char *)h2->addr.intnum <
					(unsigned char *)parent->ptrs.val_ent.children->num->ptr.lv + lv_sbs_blk_size);
				if (!IS_STP_SPACE_AVAILABLE(MAX_NUM_SIZE))
				{
					v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
					INVOKE_STP_GCOL(MAX_NUM_SIZE);
					assert((char *)stringpool.free - v1->str.addr == v1->str.len);
				}
				j = (int)(h2->addr.intnum - parent->ptrs.val_ent.children->num->ptr.lv);
				MV_FORCE_MVAL(v2, j);
				n2s(v2);
				if (last_sub_null)
					last_fnquery_return_sub[last_fnquery_return_subcnt++] = *v2;
				break;
			case SBS_BLK_TYPE_FLT:
				if (!IS_STP_SPACE_AVAILABLE(MAX_NUM_SIZE))
				{
					v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
					INVOKE_STP_GCOL(MAX_NUM_SIZE);
					assert((char *)stringpool.free - v1->str.addr == v1->str.len);
				}
				MV_ASGN_FLT2MVAL(*v2, h2->addr.flt->flt);
				n2s(v2);
				if (last_sub_null)
					last_fnquery_return_sub[last_fnquery_return_subcnt++] = *v2;
				break;
			case SBS_BLK_TYPE_STR:
				v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
				if (local_collseq)
				{
					ALLOC_XFORM_BUFF(&h2->addr.str->str);
					assert(NULL != lcl_coll_xform_buff);
					tmp_sbs.str.addr = lcl_coll_xform_buff;
					tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
					do_xform(local_collseq, XBACK, &h2->addr.str->str,
							&tmp_sbs.str, &length);
					tmp_sbs.str.len = length;
					v2->str = tmp_sbs.str;
				} else
					v2->str = h2->addr.str->str;
				/* Now that v2->str has been initialized, initialize mvtype as well (doing this in the other
				 * order could cause stp_gcol (if invoked in between) to get confused since v2->str is
				 * uninitialized (in the M-stack).
				 */
				v2->mvtype = MV_STR;
				mval_lex(v2, &format_out);
				if (format_out.addr != (char *)stringpool.free)
				{	/* We must put the string on the string pool ourself - mval_lex didn't do it
					   because v2 is a canonical numeric string. It is canonical but has too many
					   digits to be treated as a number. It must be output as a quoted string. */
					if (!IS_STP_SPACE_AVAILABLE(v2->str.len + 2))
					{
						v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
						INVOKE_STP_GCOL(v2->str.len + 2);
						assert((char *)stringpool.free - v1->str.addr == v1->str.len);
					}
					*stringpool.free++ = '\"';
					memcpy(stringpool.free, v2->str.addr, v2->str.len);
					if (last_sub_null)
					{
						last_fnquery_return_sub[last_fnquery_return_subcnt].mvtype = MV_STR;
						last_fnquery_return_sub[last_fnquery_return_subcnt].str.addr =
							(char *)stringpool.free;
						last_fnquery_return_sub[last_fnquery_return_subcnt++].str.len = v2->str.len;
					}
					stringpool.free += v2->str.len;
					*stringpool.free++ = '\"';
				} else
				{
					if (last_sub_null)
					{
						last_fnquery_return_sub[last_fnquery_return_subcnt].mvtype = MV_STR;
						last_fnquery_return_sub[last_fnquery_return_subcnt].str.addr =
							(char *)stringpool.free;
						last_fnquery_return_sub[last_fnquery_return_subcnt++].str.len = format_out.len;
					}
					stringpool.free += format_out.len;
				}
				break;
			default:
				GTMASSERT;
		}
		if (!IS_STP_SPACE_AVAILABLE(1))
		{
			v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
			INVOKE_STP_GCOL(1);
			assert((char *)stringpool.free - v1->str.addr == v1->str.len);
		}
		*stringpool.free++ = (h2 < h1 ? ',' : ')');
	}
	dst->mvtype = MV_STR;
	dst->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
	dst->str.addr = v1->str.addr;
	POP_MV_STENT();	/* v2 */
	POP_MV_STENT();	/* v1 */
}
