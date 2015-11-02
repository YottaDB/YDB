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

#include "mdef.h"

#include <stdarg.h>

#include "gtm_string.h"

#include "lv_val.h"
#include "subscript.h"
#include "stringpool.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "collseq.h"
#include "compiler.h"
#include "op.h"
#include "do_xform.h"
#include "mvalconv.h"
#include "numcmp.h"
#include "promodemo.h"	/* for "demote" prototype used in LV_NODE_GET_KEY */

GBLREF spdesc		stringpool;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;

LITREF	mval		literal_null;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);
error_def(ERR_LVNULLSUBS);

void op_fnquery(UNIX_ONLY_COMMA(int sbscnt) mval *dst, ...)
{
	int			length;
	mval		 	tmp_sbs;
	va_list			var;
	mval			*varname, *v1, *v2, *mv, tmpmv;
	mval			*arg1, **argpp, *args[MAX_LVSUBSCRIPTS], **argpp2, *lfrsbs, *argp2;
	mval			xform_args[MAX_LVSUBSCRIPTS];	/* for lclcol */
	mstr			format_out;
	lv_val			*v;
	lvTreeNode		**h1, **h2, *history[MAX_LVSUBSCRIPTS], *parent, *node, *nullsubsnode, *nullsubsparent;
	lvTree			*lvt;
	int			i, j;
	VMS_ONLY(int		sbscnt;)
	boolean_t		found, is_num, last_sub_null, nullify_term, is_str;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, dst);
	VMS_ONLY(va_count(sbscnt));
		assert(3 <= sbscnt);
	sbscnt -= 3;
	varname = va_arg(var, mval *);
	v = va_arg(var, lv_val *);
	assert(v);
	assert(LV_IS_BASE_VAR(v));
	lvt = LV_GET_CHILD(v);
	if (NULL == lvt)
	{
		dst->mvtype = MV_STR;
		dst->str.len = 0;
		return;
	}
	h1 = history;
	*h1++ = (lvTreeNode *)v;
	found = FALSE;
	DEBUG_ONLY(node = NULL;)
	for (i = 0, argpp = &args[0]; i < sbscnt; i++, argpp++, h1++)
	{
		if (NULL == lvt)
		{	/* Subtree does not exist at this level. To get $query of input node,
			 * need to start from right sibling of parent level.  */
			h1--;
			assert(h1 >= &history[1]);
			assert(NULL != node);
			assert(node == *h1);
			node = *(h1 - 1);
			lvt = LV_GET_CHILD(node);
			assert(NULL != lvt);
			break;
		}
		arg1 = *argpp = va_arg(var, mval *);
		MV_FORCE_DEFINED(arg1);
		is_str = MV_IS_STRING(arg1);
		if (is_str)
		{
			if ((0 == arg1->str.len) && (i + 1 != sbscnt) && (LVNULLSUBS_NEVER == TREF(lv_null_subs)))
			{	/* This is not the last subscript, we don't allow nulls subs and it was null */
				va_end(var);
				rts_error(VARLSTCNT(1) ERR_LVNULLSUBS);
			}
			if (is_num = MV_IS_CANONICAL(arg1))
				MV_FORCE_NUM(arg1);
			else if ((i + 1 == sbscnt) && (0 == arg1->str.len))
			{ 	/* The last search argument is a null string. For this situation, there is the possibility
				 * of a syntax collision if (1) the user had (for example) specified $Q(a(1,3,"") to get
				 * the first a(1,3,x) node or (2) this is the "next" element that was legitimately returned
				 * by $Query on the last call. If the element a(1,3,"") actually exists, the code whereby
				 * we simply strip off the last null search argument and continue will just find this
				 * element again. Because of this we save the mvals of the subscrips of the last $QUERY
				 * result if the result had a null last subscript. In that case, we will compare the input
				 * variable and subscripts with the last returned value. If they are identical, we will
				 * bypass the elimination of the trailing null subscript. This will allow the successor
				 * element to be found. Note that this is a single value that is kept for the entire
				 * process. Any intervening $QUery calls for other local variables will reset the check.
				 * SE 2/2004 (D9D08-002352).
				 */
				nullify_term = TRUE;
				if ((TREF(last_fnquery_return_varname)).str.len
					&& (TREF(last_fnquery_return_varname)).str.len == varname->str.len
					&& 0 == memcmp((TREF(last_fnquery_return_varname)).str.addr,
						varname->str.addr, varname->str.len)
					&& sbscnt == TREF(last_fnquery_return_subcnt))
				{	/* We have an equivalent varname and same number subscripts */
					for (j = 0, argpp2 = &args[0], lfrsbs = TADR(last_fnquery_return_sub);
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
					assert(i == sbscnt);
					break;
				}
			}
		} else /* not string */
		{
			assert(MV_IS_NUMERIC(arg1));
			assert(MV_IS_CANONICAL(arg1));
			is_num = TRUE;
		}
		if (!is_num)
		{
			assert(!TREE_KEY_SUBSCR_IS_CANONICAL(arg1->mvtype));
			if (TREF(local_collseq))
			{
				ALLOC_XFORM_BUFF(arg1->str.len);
				/* D9607-258 changed xback to xform and added xform_args[] to hold mval pointing to
				 * xform'd subscript which is in pool space.  tmp_sbs (which is really a mval) was being
				 * overwritten if >1 alpha subscripts */
				assert(NULL != TREF(lcl_coll_xform_buff));
				tmp_sbs.mvtype = MV_STR;
				tmp_sbs.str.addr = TREF(lcl_coll_xform_buff);
				tmp_sbs.str.len = TREF(max_lcl_coll_xform_bufsiz);
				/* KMK subscript index is i+1 */
				do_xform(TREF(local_collseq), XFORM, &arg1->str, &tmp_sbs.str, &length);
				tmp_sbs.str.len = length;
				s2pool(&(tmp_sbs.str));
				xform_args[i] = tmp_sbs;
				arg1 = &xform_args[i];
			}
			node = lvAvlTreeLookupStr(lvt, arg1, &parent);
		} else
		{
			tmp_sbs = *arg1;
			arg1 = &tmp_sbs;
			MV_FORCE_NUM(arg1);
			TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(arg1); /* used by the lvAvlTreeLookup* functions below */
			if (MVTYPE_IS_INT(arg1->mvtype))
				node = lvAvlTreeLookupInt(lvt, arg1, &parent);
			else
				node = lvAvlTreeLookupNum(lvt, arg1, &parent);
		}
		if (NULL == node)
			break; /* Key not found in tree at this level. find right sibling in "lvt" tree with key="arg1" */
		lvt = LV_GET_CHILD(node);
		*h1 = node;
	}
	va_end(var);
	assert((i != sbscnt) || !found);
	if (!found)
	{
		if (i == sbscnt)
		{
			assert(!found);
			parent = *(h1 - 1);
			assert(NULL != parent);
			lvt = LV_GET_CHILD(parent);
			if (lvt)
			{
				found = TRUE;
				nullsubsnode = TREF(local_collseq_stdnull)
					?  lvAvlTreeLookupStr(lvt, (treeKeySubscr *)&literal_null, &nullsubsparent)
					: NULL;
				node = (NULL == nullsubsnode) ? lvAvlTreeFirst(lvt) : nullsubsnode;
				assert(NULL != node);
				*h1 = node;
			} else
				--h1;
		} else
		{	/* Need to find right sibling. "lvt" is tree to search in and "arg1" is key to search for. */
			node = lvAvlTreeKeyCollatedNext(lvt, arg1);
			if (NULL != node)
			{
				*h1 = node;
				found = TRUE;
			} else	/* if "node" is still NULL, need to start searching for right siblings at the parent level. */
				h1--;
		}
	}
	if (!found)	/* separate if needed because "found" could change after the previous "if" but before this "if" */
	{
		for ( ; ; --h1)
		{
			assert(h1 >= &history[0]);
			if (h1 == &history[0])
			{
				dst->mvtype = MV_STR;
				dst->str.len = 0;
				return;
			}
			node = *h1;
			assert(NULL != node);
			/* Find the right sibling (next) subscript */
			node = lvAvlTreeNodeCollatedNext(node);
			if (NULL != node)
			{
				*h1 = node;
				break;
			}
		}
	}
	/* Saved last query result is irrelevant now */
	TREF(last_fnquery_return_subcnt) = 0;
	(TREF(last_fnquery_return_varname)).mvtype = MV_STR;
	(TREF(last_fnquery_return_varname)).str.len = 0;
	/* Go down leftmost subtree path (potentially > 1 avl trees) starting from "node" until you find the first DEFINED mval */
	while (!LV_IS_VAL_DEFINED(node))
	{
		lvt = LV_GET_CHILD(node);
		assert(NULL != lvt);	/* there cannot be an undefined lv node with no children dangling around */
		nullsubsnode = TREF(local_collseq_stdnull)
				? lvAvlTreeLookupStr(lvt, (treeKeySubscr *)&literal_null, &nullsubsparent)
				: NULL;
		node = (NULL == nullsubsnode) ? lvAvlTreeFirst(lvt) : nullsubsnode;
		assert(NULL != node);
		*++h1 = node;
	}
	/* Before we start formatting for output, decide whether we will be saving mvals of our subscripts
	 * as we format. We only do this if the last subscript is a null. Bypassing it otherwise is a time saver.
	 */
	last_sub_null = LV_NODE_KEY_IS_NULL_SUBS(node);
	/* format the output string */
	ENSURE_STP_FREE_SPACE(varname->str.len + 1);
	PUSH_MV_STENT(MVST_MVAL);
	v1 = &mv_chain->mv_st_cont.mvs_mval;
	v1->mvtype = MV_STR;
	v1->str.len = 0;
	v1->str.addr = (char *)stringpool.free;
	PUSH_MV_STENT(MVST_MVAL);
	v2 = &mv_chain->mv_st_cont.mvs_mval;
	v2->mvtype = 0;	/* initialize it to 0 to avoid "stp_gcol" from getting confused if it gets invoked before v2 has been
			 * completely setup. */
	memcpy(stringpool.free, varname->str.addr, varname->str.len);
	if (last_sub_null)
	{
		(TREF(last_fnquery_return_varname)).str.addr = (char *)stringpool.free;
		(TREF(last_fnquery_return_varname)).str.len += varname->str.len;
	}
	stringpool.free += varname->str.len;
	*stringpool.free++ = '(';
	for (h2 = &history[1]; h2 <= h1; h2++)
	{
		node = *h2;
		assert(!LV_IS_BASE_VAR(node)); /* guarantees to us that "node" is a "lvTreeNode *" and not "lv_val *" */
		mv = &tmpmv;
		LV_NODE_GET_KEY(node, mv); /* Get node key into "mv" depending on the structure type of "node" */
		if (MV_IS_NUMERIC(mv))
		{	/* number */
			if (!IS_STP_SPACE_AVAILABLE(MAX_NUM_SIZE))
			{
				v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
				INVOKE_STP_GCOL(MAX_NUM_SIZE);
				assert((char *)stringpool.free - v1->str.addr == v1->str.len);
			}
			*v2 = *mv;
			/* Now that we have ensured enough space in the stringpool, we dont expect any more
			 * garbage collections or expansions until we are done with the n2s.
			 */
			DBG_MARK_STRINGPOOL_UNEXPANDABLE;
			n2s(v2);
			/* Now that we are done with any stringpool.free usages, mark as free for expansion */
			DBG_MARK_STRINGPOOL_EXPANDABLE;
			if (last_sub_null)
				TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))++) = *v2;
		} else
		{	/* string */
			assert(MV_IS_STRING(mv));
			v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
			v2->mvtype = 0;	/* initialize it to 0 to avoid "stp_gcol" from getting confused
					 * if it gets invoked before v2 has been completely setup. */
			if (TREF(local_collseq))
			{
				ALLOC_XFORM_BUFF(mv->str.len);
				assert(NULL != TREF(lcl_coll_xform_buff));
				tmp_sbs.str.addr = TREF(lcl_coll_xform_buff);
				tmp_sbs.str.len = TREF(max_lcl_coll_xform_bufsiz);
				do_xform(TREF(local_collseq), XBACK, &mv->str, &tmp_sbs.str, &length);
				tmp_sbs.str.len = length;
				v2->str = tmp_sbs.str;
			} else
				v2->str = mv->str;
			/* Now that v2->str has been initialized, initialize mvtype as well (doing this in the other
			 * order could cause "stp_gcol" (if invoked in between) to get confused since v2->str is
			 * not yet initialized with current subscript (in the M-stack).
			 */
			v2->mvtype = MV_STR;
			mval_lex(v2, &format_out);
			if (format_out.addr != (char *)stringpool.free)	/* BYPASSOK */
			{	/* We must put the string on the string pool ourself - mval_lex didn't do it
				 * because v2 is a canonical numeric string. It is canonical but has too many
				 * digits to be treated as a number. It must be output as a quoted string. */
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
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).mvtype = MV_STR;
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).str.addr =
						(char *)stringpool.free;
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))++).str.len =
						v2->str.len;
				}
				stringpool.free += v2->str.len;
				*stringpool.free++ = '\"';
			} else
			{
				if (last_sub_null)
				{
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).mvtype = MV_STR;
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).str.addr =
						(char *)stringpool.free;
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))++).str.len =
						format_out.len;
				}
				stringpool.free += format_out.len;
			}
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
