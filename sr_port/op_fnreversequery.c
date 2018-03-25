/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Code in this module is based on op_fnquery.c and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

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
#include "libyottadb_int.h"

GBLREF spdesc		stringpool;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;

LITREF	mval		literal_null;

/* Similar to IS_IN_STRINGPOOL macro in stringpool.h but we only need to check the start address and can assume the
 * remainder of the string is in the stringpool (saves some arithmetic)
 */
#define	IS_ADDR_IN_STRINGPOOL(PTR) ((((unsigned char *)PTR) < stringpool.top) && ((unsigned char *)PTR >= stringpool.base))

error_def(ERR_LVNULLSUBS);
error_def(ERR_MAXSTRLEN);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

/* This function is the runtime entry point for $query(lvn,-1) where the -1 is known at compile time.
 * It is passed a variable number of parameters (corresponding to the potentially subscripted lvn).
 * Get a varargs list started for that part. And invoke another function "op_fnreversequery_va" as that is also
 * used by "op_fnq2" which is the runtime entry point for $query(lvn,dir) where dir is an expression evaluating to -1.
 */
void op_fnreversequery(int sbscnt, mval *dst, ...)
{
	va_list		var;

	VAR_START(var, dst);
	op_fnreversequery_va(sbscnt, dst, var);
}

/* This function implements reverse $query for a lvn.
 * sbscnt is the count of the # of parameters including
 *	sbscnt,
 *	varname, (the base variable name which is the first parameter in the "var" varargs list)
 *	v,       (the lv_val corresponding to the base local variable)
 *	# of subscripts in the lvn if any
 * dst is the destination mval where the $query return is placed
 *
 * Also note that the general flow below is similar to that of op_fnquery.c.
 *
 * Note - this module runs in two modes:
 *   1. Normal calls from generated code on behalf of a $QUERY() usage.
 *   2. Calls to ydb_node_previous_s() from the SimpleAPI.
 * The IS_SIMPLEAPI_MODE macro can determine which mode we are in. The mode regulates how this routine returns its
 * output to the caller. The simpleAPI "returns" its information into a static array allocated at need with the addr
 * cached in TREF(sapi_query_node_subs) with the count of valid entries in TREF(sapi_query_node_subs_cnt) while a
 * YDB runtime call returns a string in the dst mval.
*/
void op_fnreversequery_va(int sbscnt, mval *dst, va_list var)
{
	int			length, dstlen;
	mval			tmp_sbs, *last_fnquery_ret;
	mval			*varname, *v1, *v2, *mv, tmpmv;
	mval			*arg1, **argpp, *args[MAX_LVSUBSCRIPTS], **argpp2, *lfrsbs, *argp2;
	mval			xform_args[MAX_LVSUBSCRIPTS];	/* for lclcol */
	mstr			format_out, *retsub;
	lv_val			*v;
	lvTreeNode		**h1, **h2, *history[MAX_LVSUBSCRIPTS], *parent, *node, *nullsubsnode, *nullsubsparent;
	lvTree			*lvt, *tmp_lvt;
	int			i, j, nexti;
	boolean_t		descend, is_num, last_sub_null, nullsubs_implies_lastsub, is_str, push_v1, is_simpleapi_mode;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	push_v1 = FALSE;			/* v1 mv_stent assumed not used (v2 always is unless short-return) */
	TREF(sapi_query_node_subs_cnt) = 0;
	is_simpleapi_mode = IS_SIMPLEAPI_MODE;	/* Create local flag value */
	assert(3 <= sbscnt);
	sbscnt -= 3;	/* Take away sbscnt, varname and v from sbscnt and you will get the # of subscripts in the lvn */
	if (!sbscnt)
	{	/* Reverse $query of unsubscripted base local variable name is always the null string */
		if (!is_simpleapi_mode)
		{	/* Return array size of zero is the signal there is nothing else */
			dst->mvtype = MV_STR;
			dst->str.len = 0;
		}
		return;
	}
	varname = va_arg(var, mval *);
	v = va_arg(var, lv_val *);
	assert(v);
	assert(LV_IS_BASE_VAR(v));
	lvt = LV_GET_CHILD(v);
	h1 = history;
	*h1 = (lvTreeNode *)v;
	if (NULL != lvt)
	{	/* There is at least one subscripted node under the base lv. Traverse the lv tree searching for the
		 * input subscripted lvn and later use the located node to find its left sibling (if one exists) or
		 * a parent to determine the reverse $query result.
		 */
		DEBUG_ONLY(node = NULL);
		h1++;
		for (i = 0, argpp = &args[0]; ; argpp++, h1++)
		{
			arg1 = *argpp = va_arg(var, mval *);
			MV_FORCE_DEFINED(arg1);
			is_str = MV_IS_STRING(arg1);
			nexti = i + 1;
			if (is_str)
			{
				if ((0 == arg1->str.len) && (nexti != sbscnt) && (LVNULLSUBS_NEVER == TREF(lv_null_subs)))
				{	/* This is not the last subscript, we don't allow nulls subs and it was null */
					va_end(var);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LVNULLSUBS);
				}
				if (is_num = MV_IS_CANONICAL(arg1))
					MV_FORCE_NUM(arg1);
				else if ((nexti == sbscnt) && (0 == arg1->str.len))
				{	/* The last search argument is a null string. See op_fnquery for comment on this situation.
					 * Like there, we will compare the input variable and subscripts with the last returned
					 * value. If they are identical, we will bypass the special meaning of the trailing null
					 * subscript (to find last subscript at that level). This will allow the predecessor
					 * element to be found. Note that this is a single value that is kept for the entire
					 * process. Any intervening $QUERY calls for other local variables will reset the check.
					 * Also note that only this single value is kept across $query(,1) and $query(,-1) calls
					 * (i.e. forward and reverse $query) for any local variable in this process.
					 */
					last_fnquery_ret = &TREF(last_fnquery_return_varname);
					if (last_fnquery_ret->str.len
					    && (last_fnquery_ret->str.len == varname->str.len)
					    && (0 == memcmp(last_fnquery_ret->str.addr, varname->str.addr, varname->str.len))
					    && (sbscnt == TREF(last_fnquery_return_subcnt)))
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
								if (((argp2)->str.len == lfrsbs->str.len)
								    && (0 != memcmp((argp2)->str.addr, lfrsbs->str.addr,
										    lfrsbs->str.len)))
									break;	/* This subscript isn't the same */
							} else
								break;		/* This subscript isn't even close.. */
						}
						nullsubs_implies_lastsub = (j != i);
					} else
						nullsubs_implies_lastsub = TRUE;
					if (nullsubs_implies_lastsub)
					{
						node = lvAvlTreeCollatedLast(lvt);
						assert(NULL != node);
						*h1 = node;
						descend = TRUE;
						break;
					}
				}
			} else /* Not string */
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
					 * overwritten if >1 alpha subscripts.
					 */
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
				TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(arg1);	/* Used by the lvAvlTreeLookup* functions below */
				if (MVTYPE_IS_INT(arg1->mvtype))
					node = lvAvlTreeLookupInt(lvt, arg1, &parent);
				else
					node = lvAvlTreeLookupNum(lvt, arg1, &parent);
			}
			if (NULL != node)
			{
				tmp_lvt = LV_GET_CHILD(node);
				*h1 = node;
				assert(nexti <= sbscnt);
				i = nexti;
			}
			if ((NULL == node) || (NULL == tmp_lvt) || (i == sbscnt))
			{	/* (NULL == node)    ==> Key not found in tree at this level
				 * (NULL == tmp_lvt) ==> Subtree does not exist at this level
				 * (i == sbscnt)     ==> At last specified input lvn subscript level
				 * For all 3 cases, to get reverse $query of input node, need to start from left sibling of
				 * parent level. Parent level is still "lvt" tree with key="arg1"
				 */
				node = lvAvlTreeKeyCollatedPrev(lvt, arg1);
				if (NULL != node)
				{
					*h1 = node;
					descend = TRUE;
					break;
				}
				/* else: "node" is still NULL. Need to start searching for left siblings at the parent level. */
				h1--;
				assert(h1 >= &history[0]);
				assert(NULL != *h1);
				descend = FALSE;
				break;
			}
			lvt = tmp_lvt;
		}
		assert(!descend || (NULL != node));
	} else
		descend = FALSE;
	va_end(var);
	if (!descend)
	{	/* Ascend up the tree at "node" until you find a defined node or a left sibling */
		do
		{
			if (h1 == &history[0])
			{
				assert(*h1 == (lvTreeNode *)v);
				if (!LV_IS_VAL_DEFINED(v))
				{	/* Neither the base variable nor a tree underneath it exists. Return value is null string */
					if (!is_simpleapi_mode)
					{	/* Array size of zero is the signal there is nothing else */
						dst->mvtype = MV_STR;
						dst->str.len = 0;
					}
				} else
				{	/* Base variable exists. Return value from reverse $query is base variable name.
					 * Saved last query result (if any) is irrelevant now.
					 */
					TREF(last_fnquery_return_subcnt) = 0;
					(TREF(last_fnquery_return_varname)).mvtype = MV_STR;
					(TREF(last_fnquery_return_varname)).str.len = 0;
					if (!is_simpleapi_mode)
					{
						ENSURE_STP_FREE_SPACE(varname->str.len);
						memcpy(stringpool.free, varname->str.addr, varname->str.len);
						dst->mvtype = MV_STR;
						dst->str.addr = (char *)stringpool.free;
						dst->str.len = varname->str.len;
						stringpool.free += varname->str.len;
					} else
					{	/* Returning just the name (no subscripts) so set the subscript count to -1 to
						 * differentiate it from a NULL return signaling the end of the list.
						 */
						TREF(sapi_query_node_subs_cnt) = -1;
					}
				}
				return;
			}
			node = *h1;
			assert(NULL != node);
			if (LV_IS_VAL_DEFINED(node))
				break;		/* This node is the $query return value (no more descends needed) */
			node = lvAvlTreeNodeCollatedPrev(node); /* Find the left sibling (previous) subscript */
			if (NULL != node)
			{
				*h1 = node;
				descend = TRUE;
				break;
			}
			h1--;
		} while (TRUE);
	}
	if (descend)
	{	/* Go down rightmost subtree path (potentially > 1 avl trees) starting from "node"
		 * until you find the first DEFINED mval.
		 */
		assert(*h1 == node);
		do
		{
			lvt = LV_GET_CHILD(node);
			if (NULL == lvt)
				break;
			node = lvAvlTreeCollatedLast(lvt);
			assert(NULL != node);
			*++h1 = node;
		} while (TRUE);
	}
	/* Saved last query result is irrelevant now */
	TREF(last_fnquery_return_subcnt) = 0;
	(TREF(last_fnquery_return_varname)).mvtype = MV_STR;
	(TREF(last_fnquery_return_varname)).str.len = 0;
	/* Before we start formatting for output, decide whether we will be saving mvals of our subscripts
	 * as we format. We only do this if the last subscript is a null. Bypassing it otherwise is a time saver.
	 */
	last_sub_null = LV_NODE_KEY_IS_NULL_SUBS(node);
	/* The actual return of the next node differs significantly depending on whether this is a $QUERY() call from
	 * generated code or a ydb_node_next_s() call from the simpleAPI. Fork that difference here.
	 */
	if (is_simpleapi_mode)
	{	/* SimpleAPI mode - This routine returns (in a C global variable) a list of mstr blocks describing
		 * the subscripts to return. These are later (different routine) used to populate the caller's
		 * ydb_buffer_t array. Also, all values are returned as (unquoted) strings eliminating some
		 * display formatting issues. That all said, we do need at least one protected mval so we can
		 * run n2s to convert numeric subscripts to the string values we need to return.
		 */
		assert(NULL == dst);		/* Output is via TREF(sapi_query_node_subs) instead */
		PUSH_MV_STENT(MVST_MVAL);
		v2 = &mv_chain->mv_st_cont.mvs_mval;
		v2->mvtype = 0;	/* Initialize it to 0 to avoid "stp_gcol" from getting confused if it gets invoked before v2 has
				 * been completely setup.
				 */
		if (last_sub_null)
		{
			if (!IS_ADDR_IN_STRINGPOOL(varname->str.addr))
			{	/* Need to move to stringpool for safe-keeping */
				ENSURE_STP_FREE_SPACE(varname->str.len);
				memcpy(stringpool.free, varname->str.addr, varname->str.len);
				(TREF(last_fnquery_return_varname)).str.addr = (char *)stringpool.free;
				stringpool.free += varname->str.len;
			} else
				(TREF(last_fnquery_return_varname)).str.addr = varname->str.addr;
			(TREF(last_fnquery_return_varname)).str.len = varname->str.len;
		}
		/* Verify global subscript array is available and if not make it so */
		if (NULL == TREF(sapi_query_node_subs))
			/* Allocate mstr array we need */
			TREF(sapi_query_node_subs) = malloc(SIZEOF(mstr) * YDB_MAX_SUBS);
		for (h2 = &history[1], retsub = TREF(sapi_query_node_subs); h2 <= h1; h2++)
		{
			node = *h2;
			assert(!LV_IS_BASE_VAR(node)); /* guarantees to us that "node" is a "lvTreeNode *" and not "lv_val *" */
			mv = &tmpmv;
			LV_NODE_GET_KEY(node, mv); /* Get node key into "mv" depending on the structure type of "node" */
			if (MV_IS_NUMERIC(mv))
			{	/* Number */
				ENSURE_STP_FREE_SPACE(MAX_NUM_SIZE);
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
			{	/* String */
				assert(MV_IS_STRING(mv));
				v2->mvtype = 0;	/* Initialize it to 0 to avoid "stp_gcol" from getting confused
						 * if it gets invoked before v2 has been completely setup.
						 */
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
				/* At this point, v2 has been setup to contain the subscript value to return. Make sure
				 * it lives in the stringpool so it is protected and survives the trip back to our
				 * caller.
				 */
				v2->mvtype = MV_STR;		/* Now has an active value we need for a bit yet */
				if (!IS_ADDR_IN_STRINGPOOL(v2->str.addr))
					s2pool(&v2->str);
				/* Save subscripts to identify a loop in case last subscript value is actually NULL */
				if (last_sub_null)
				{
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).mvtype = MV_STR;
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).str.addr =
						v2->str.addr;
					TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))++).str.len =
						v2->str.len;
				}
			}
			/* Save a copy of the mstr in our global subscript structure we'll return to our caller */
			*retsub++ = v2->str;
			(TREF(sapi_query_node_subs_cnt))++;
		}
	} else
	{	/* Call from M code runtime - Format the output string for return to M code */
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
					assert(IS_AT_END_OF_STRINGPOOL(v1->str.addr, v1->str.len));
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
						TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).str.addr
							= (char *)stringpool.free;
						TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))++).str.len
							= v2->str.len;
					}
					stringpool.free += v2->str.len;
					*stringpool.free++ = '\"';
				} else
				{
					if (last_sub_null)
					{
						TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).mvtype = MV_STR;
						TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))).str.addr
							= (char *)stringpool.free;
						TAREF1(last_fnquery_return_sub,(TREF(last_fnquery_return_subcnt))++).str.len
							= format_out.len;
					}
					stringpool.free += format_out.len;
				}
			}
			if (!IS_STP_SPACE_AVAILABLE(1))
			{
				v1->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
				INVOKE_STP_GCOL(1);
				assert(IS_AT_END_OF_STRINGPOOL(v1->str.addr, v1->str.len));
			}
			*stringpool.free++ = (h2 < h1 ? ',' : ')');
		}
		dstlen = INTCAST((char *)stringpool.free - v1->str.addr);
		if (MAX_STRLEN < dstlen)
		{	/* Result of $query would be greater than maximum string length allowed. Error out but cleanup before
			 * driving the error.
			 */
			stringpool.free = (unsigned char *)v1->str.addr; /* Remove incomplete $query result from stringpool */
			if (last_sub_null)
			{	/* If TREF(last_fnquery_return_subcnt) was being maintained above, reset it too */
				TREF(last_fnquery_return_subcnt) = 0;
				(TREF(last_fnquery_return_varname)).mvtype = MV_STR;
				(TREF(last_fnquery_return_varname)).str.len = 0;
			}
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
		}
		dst->mvtype = MV_STR;
		dst->str.len = INTCAST((char *)stringpool.free - v1->str.addr);
		dst->str.addr = v1->str.addr;
	}
	POP_MV_STENT();		/* v2 - this one was always allocated */
	if (push_v1)
	{
		POP_MV_STENT();	/* v1 - this one only allocated in the M runtime path (not simpleAPI path) */
	}
}
