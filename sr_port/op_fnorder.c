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

#include "lv_val.h"
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "numcmp.h"
#include "mvalconv.h"
#include "promodemo.h"	/* for "demote" prototype used in LV_NODE_GET_KEY */

#define MINUS_ONE -MV_BIAS

error_def(ERR_NUMOFLOW);

void op_fnorder(lv_val *src, mval *key, mval *dst)
{
	mval		tmp_sbs;
	int             length;
	boolean_t	is_canonical, is_fnnext, get_first;
	lvTree		*lvt;
	lvTreeNode	*node;
	uint4		mvt;	/* Local copy of mvtype, bit ands use a int4, so do conversion once */
	mstr		*str;
	int4		intval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	is_fnnext = TREF(in_op_fnnext);
	TREF(in_op_fnnext) = FALSE;
	if (src && (lvt = LV_GET_CHILD(src)))	/* caution: assignment */
	{
		MV_FORCE_DEFINED(key);
		/* If last subscript is null, $ORDER returns the first subscript in that level.
		 * With the obsoleted $NEXT function, a subscript of -1 also triggers the same behavior.
		 */
		get_first = FALSE;
		if (MV_IS_STRING(key) && (0 == key->str.len))
			get_first = TRUE;
		else if (is_fnnext)
		{
			mvt = key->mvtype;
			if (!(mvt & (MV_NM | MV_NUM_APPROX)))
			{	/* Not currently in numeric form.  Is it cannonical? */
				if (val_iscan(key))
				{	/* Yes, convert it to numeric */
					(void)s2n(key);
					mvt = key->mvtype;
					if (!(mvt & MV_NM))
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
				} else	/* No, not numeric.  Note the fact for future reference */
					mvt = key->mvtype |= MV_NUM_APPROX;
			}
			if (MV_IS_TRUEINT(key, &intval) && (MINUS_ONE == key->m[1]))
				get_first = TRUE;
		}
		if (get_first)
			node = lvAvlTreeFirst(lvt);
		else
		{
			is_canonical = MV_IS_CANONICAL(key);
			if (!is_canonical)
			{
				assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
				if (TREF(local_collseq))
				{
					ALLOC_XFORM_BUFF(key->str.len);
					tmp_sbs.mvtype = MV_STR;
					tmp_sbs.str.len = TREF(max_lcl_coll_xform_bufsiz);
					assert(NULL != TREF(lcl_coll_xform_buff));
					tmp_sbs.str.addr = TREF(lcl_coll_xform_buff);
					do_xform(TREF(local_collseq), XFORM, &key->str, &tmp_sbs.str, &length);
					tmp_sbs.str.len = length;
					s2pool(&(tmp_sbs.str));
					key = &tmp_sbs;
				}
			} else
			{	/* Need to set canonical bit before calling tree search functions.
				 * But input mval could be read-only so cannot modify that even if temporarily.
				 * So take a copy of the mval and modify that instead.
				 */
				tmp_sbs = *key;
				key = &tmp_sbs;
				MV_FORCE_NUM(key);
				TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(key);	/* used by the lvTreeKeyNext function */
			}
			node = lvAvlTreeKeyNext(lvt, key);
		}
		/* If STDNULLCOLL, skip to the next subscript should the current subscript be "" */
		if (TREF(local_collseq_stdnull) && (NULL != node) && LV_NODE_KEY_IS_NULL_SUBS(node))
		{
			assert(LVNULLSUBS_OK == TREF(lv_null_subs));
			node = lvAvlTreeNext(node);
		}
	} else
		node = NULL;
	if (NULL == node)
	{
		if (!is_fnnext)
		{
			dst->mvtype = MV_STR;
			dst->str.len = 0;
		} else
			MV_FORCE_MVAL(dst, -1);
	} else
	{
		LV_NODE_GET_KEY(node, dst); /* Get node key into "dst" depending on the structure type of "node" */
		/* Code outside lv_tree.c does not currently know to make use of MV_CANONICAL bit so reset it
		 * until the entire codebase gets fixed to maintain MV_CANONICAL bit accurately at which point,
		 * this RESET can be removed */
		TREE_KEY_SUBSCR_RESET_MV_CANONICAL_BIT(dst);
		if (TREF(local_collseq) && MV_IS_STRING(dst))
		{
			ALLOC_XFORM_BUFF(dst->str.len);
			assert(NULL != TREF(lcl_coll_xform_buff));
			tmp_sbs.str.addr = TREF(lcl_coll_xform_buff);
			tmp_sbs.str.len = TREF(max_lcl_coll_xform_bufsiz);
			do_xform(TREF(local_collseq), XBACK, &dst->str, &tmp_sbs.str, &length);
			tmp_sbs.str.len = length;
			s2pool(&(tmp_sbs.str));
			dst->str = tmp_sbs.str;
		}
	}
}
