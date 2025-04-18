/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include <stddef.h> 		/* for offsetof */
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "lv_val.h"
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "mvalconv.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "compiler.h"
#include "callg.h"
#include "toktyp.h"
#include "valid_mname.h"
#include "stack_frame.h"

#ifdef DEBUG
#define	ACTIVELV_DBG_ARRAY_SIZE_DEF	64
#endif

GBLREF symval		*curr_symval;
GBLREF stack_frame	*frame_pointer;
GBLREF bool		undef_inhibit;
#ifdef DEBUG
GBLREF	lv_val		*active_lv;
#endif

error_def(ERR_LVNULLSUBS);
error_def(ERR_MAXSTRLEN);

#ifdef DEBUG
void	set_active_lv(lv_val *newlv, boolean_t do_assert, int type)
{
	activelv_dbg_t	*dbg_array;
	int		lv_index;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == (dbg_array = TREF(activelv_dbg_array)))
	{
		TREF(activelv_dbg_array) = (activelv_dbg_t *)malloc(ACTIVELV_DBG_ARRAY_SIZE_DEF * SIZEOF(activelv_dbg_t));
		dbg_array = TREF(activelv_dbg_array);
	}
	if (do_assert)
		ASSERT_ACTIVELV_GOOD(active_lv);
	if (NULL != newlv)
		assert(!LV_IS_BASE_VAR(newlv));	/* active_lv should never be set to a base variable */
	lv_index = TREF(activelv_index);
	assert((0 <= lv_index) && (ACTIVELV_DBG_ARRAY_SIZE_DEF > lv_index));
	if ((active_lv != newlv) || (0 == TREF(activelv_cycle)))
	{
		(TREF(activelv_cycle))++;
		(TREF(activelv_index))++;
		if (ACTIVELV_DBG_ARRAY_SIZE_DEF == TREF(activelv_index))
			TREF(activelv_index) = 0;
		dbg_array[lv_index].active_lv = active_lv;
		dbg_array[lv_index].newlv = newlv;
		dbg_array[lv_index].count = 0;
	} else
		dbg_array[lv_index].count++;
	dbg_array[lv_index].type = type;
	dbg_array[lv_index].frame_pointer = frame_pointer;
	dbg_array[lv_index].curr_symval = curr_symval;
	dbg_array[lv_index].mpc = (NULL != frame_pointer) ? frame_pointer->mpc : NULL;
	dbg_array[lv_index].ctxt = (NULL != frame_pointer) ? frame_pointer->ctxt : NULL;
	active_lv = newlv;
}
#endif

lv_val	*op_putindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...)
{
	boolean_t 		is_canonical, is_base_var;
	int			length, subs_level;
	lv_val			*lv;
	mval			*key, tmp_sbs;
	va_list			var;
	lvTree			*lvt;
	lvTreeNode		*parent;
	lv_val			*base_lv;
	DEBUG_ONLY(int		orig_subs_level;)
	VMS_ONLY(int		argcnt;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, start);
	VMS_ONLY(va_count(argcnt);)
	assert(0 < argcnt);
	is_base_var = LV_IS_BASE_VAR(start);
	/* If this variable is marked as a Transaction Processing protected variable, clone the tree.
	 * It is possible the input "start" is NOT a base lv_val. In that case, we want to make sure that
	 * the base lv_val, if it needs to be preserved against tp restarts, has already cloned a copy before
	 * we go about modifying "start" here (it is the caller's responsibility to call op_putindx with
	 * the base lv_val before calling it with a subscripted lv_val). Assert that below.
	 */
	if (is_base_var)
	{
		base_lv = start;
		if ((NULL != start->tp_var) && !start->tp_var->var_cloned)
			TP_VAR_CLONE(start);
	} else
	{
		base_lv = LV_GET_BASE_VAR(start);
		assert((NULL == base_lv->tp_var) || base_lv->tp_var->var_cloned);
	}
	lv = start;
	assert(NULL != lv);
	LV_SBS_DEPTH(start, is_base_var, subs_level);
	DEBUG_ONLY(orig_subs_level = subs_level;)
	for (subs_level++; --argcnt > 0; subs_level++)
	{
		key = va_arg(var, mval *);
		MV_FORCE_DEFINED(key);	/* Subscripts for set shouldn't be undefined - check here enables lvnullsubs to work */
		if (!(is_canonical = MV_IS_CANONICAL(key)))
		{
			assert(MV_IS_STRING(key));
			assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
			/* If input "key" is $ZYSQLNULL, then do not do NULLSUBS checks or collation transformations */
			if (!MV_IS_SQLNULL(key))
			{
				if (!key->str.len)
				{
					if (LVNULLSUBS_OK != TREF(lv_null_subs))	/* Error for both LVNULLSUBS_{NO,NEVER} */
					{
						va_end(var);
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_LVNULLSUBS);
					}
				}
				if (TREF(local_collseq))
				{	/* Do collation transformations */
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
			}
			if ((lvt = LV_GET_CHILD(lv)))	/* caution: assignment */
				assert(MV_LV_TREE == lvt->ident);
			else	/* No children exist at this level - create a child */
				LV_TREE_CREATE(lvt, (lvTreeNode *)lv, subs_level, base_lv);
			lv = (lv_val *)lvAvlTreeLookupStr(lvt, key, &parent);
		} else
		{	/* Need to set canonical bit before calling tree search functions.
			 * But input mval could be read-only so cannot modify that even if temporarily.
			 * So take a copy of the mval and modify that instead.
			 */
			assert(!MV_IS_SQLNULL(key));
			tmp_sbs = *key;
			key = &tmp_sbs;
			MV_FORCE_NUM(key);
			TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(key);	/* used by the lvAvlTreeLookup* functions below */
			/* Since this mval has the MV_CANONICAL bit set, reset MV_STR bit in case it is set.
			 * This way there is no confusion as to the type of this subscript.
			 * Also helps with stp_gcol since we dont have the tree node hanging on to the string.
			 * There is code (e.g. op_fnascii) that expects MV_UTF_LEN bit to be set only if MV_STR is set.
			 * Since we are turning off MV_STR, turn off MV_UTF_LEN bit also in case it is set.
			 */
			tmp_sbs.mvtype &= (MV_STR_OFF & MV_UTF_LEN_OFF);
			if ((lvt = LV_GET_CHILD(lv)))	/* caution: assignment */
				assert(MV_LV_TREE == lvt->ident);
			else	/* No children exist at this level - create a child */
				LV_TREE_CREATE(lvt, (lvTreeNode *)lv, subs_level, base_lv);
			if (MVTYPE_IS_INT(tmp_sbs.mvtype))
				lv = (lv_val *)lvAvlTreeLookupInt(lvt, key, &parent);
			else
				lv = (lv_val *)lvAvlTreeLookupNum(lvt, key, &parent);
		}
		if (NULL == lv)
		{
			lv = (lv_val *)lvAvlTreeNodeInsert(lvt, key, parent);
			lv->v.mvtype = 0;	/* initialize mval to undefined value at this point */
			/* maintain active_lv so we free this lv_val (and parent lv_vals as appropriate) in case of a
			 * runtime error (e.g. LVUNDEF or LVNULLSUBS) in this for loop.
			 */
			SET_ACTIVE_LV(lv, (orig_subs_level == (subs_level + 1)) ? TRUE : FALSE, actlv_op_putindx1);
		}
	}
	va_end(var);
	if (base_lv != lv)
	{
		/* This var is about to be set/modified. If it exists and is an alias container var,
		 * that reference is going to go away. Take care of that possibility now.
		 */
		if (LV_IS_VAL_DEFINED(lv))
		{
			DECR_AC_REF(lv, TRUE);
			lv->v.mvtype &= ~MV_ALIASCONT;	/* Value being replaced is now no longer a container var */
		}
		assert(NULL != lv);
		SET_ACTIVE_LV(lv, (orig_subs_level == (subs_level + 1)) ? TRUE : FALSE, actlv_op_putindx2);
	}
	return lv;
}
