/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "lv_val.h"
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "undx.h"
#include "mvalconv.h"
#include "op.h"
#include "min_max.h"

#define IS_INTEGER 0

GBLREF bool		undef_inhibit;
GBLREF symval		*curr_symval;
LITREF mval		literal_null ;

error_def(ERR_UNDEF);
error_def(ERR_LVNULLSUBS);

/* op_getindx_runtime should generally be maintained in parallel */
lv_val	*op_getindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...)
{
	VMS_ONLY(int		argcnt;)
	boolean_t		is_canonical;
	int			arg1;
	int                     length;
	mval			*key;
	mval			tmp_sbs;
	lvTree			*lvt;
	lvTreeNode		*lv, *parent;
	unsigned char		buff[512], *end;
	va_list			var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, start);
	VMS_ONLY(va_count(argcnt));
	lv = (lvTreeNode *)start;
	arg1 = --argcnt;
	while (lv && (0 < argcnt--))
	{
		key = va_arg(var, mval *);
		lvt = LV_GET_CHILD(lv);
		if (NULL == lvt)
		{
			lv = NULL;
			break;
		}
		MV_FORCE_DEFINED(key);
		is_canonical = MV_IS_CANONICAL(key);
		if (!is_canonical)
		{
			assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
			assert(MV_IS_STRING(key));
			if ((0 == key->str.len) && (LVNULLSUBS_NEVER == TREF(lv_null_subs)))
			{
				va_end(var);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_LVNULLSUBS);
			}
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
			lv = lvAvlTreeLookupStr(lvt, key, &parent);
		} else
		{	/* Need to set canonical bit before calling tree search functions.
			 * But input mval could be read-only so cannot modify that even if temporarily.
			 * So take a copy of the mval and modify that instead.
			 */
			tmp_sbs = *key;
			key = &tmp_sbs;
			MV_FORCE_NUM(key);
			TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(key);	/* used by the lvAvlTreeLookup* functions below */
			if (MVTYPE_IS_INT(tmp_sbs.mvtype))
				lv = lvAvlTreeLookupInt(lvt, key, &parent);
			else
				lv = lvAvlTreeLookupNum(lvt, key, &parent);
		}
	}
	va_end(var);
	if (!lv || !LV_IS_VAL_DEFINED(lv))
	{
		if (undef_inhibit)
			lv = (lvTreeNode *)&literal_null;
		else
		{
			VAR_START(var, start);
			end = undx(start, var, arg1, buff, SIZEOF(buff));
			va_end(var);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_UNDEF, 2, end - buff, buff);
		}
	}
	return (lv_val *)lv;
}

/* op_getindx should generally be maintained in parallel */
lv_val	*op_getindx_runtime(mval *src, int subscripts, int *start, int *stop, lv_val *ve)
{
	boolean_t		is_canonical;
	int			i;
	int                     length;
	mval			*key;
	mval			tmp_sbs;
	lvTree			*lvt;
	lvTreeNode		*lv, *parent;
	mname_entry		lvent;
	mval			*val, *varname, lvname_mval, subs_mval;
	ht_ent_mname            *tabent;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 <= subscripts);
	varname = &lvname_mval;
	val = src;
	op_fnqsubscript_fast(val, 0, varname, subscripts, start[0], stop[0]);       /* 0 : for the unsubscripted name */
	lvent.var_name.len = MIN(varname->str.len, MAX_MIDENT_LEN);
	lvent.var_name.addr = varname->str.addr;
	COMPUTE_HASH_MNAME(&lvent);
	if ((tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &lvent)))
	{       /* if the variable exists find out if it has children */
		ve = (lv_val *)tabent->value;
		assert(ve);
		lv = (lvTreeNode *)ve;
	} else
		return NULL;
	for (i = 1; lv && (i <= subscripts); i++)
	{
		key = &subs_mval;	/* reinitialize each iteration to avoid tmp_sbs corruption */
		op_fnqsubscript_fast(val, i, key, subscripts, start[i], stop[i]);
		lvt = LV_GET_CHILD(lv);
		if (NULL == lvt)
		{
			lv = NULL;
			break;
		}
		MV_FORCE_DEFINED(key);
		is_canonical = MV_IS_CANONICAL(key);
		if (!is_canonical)
		{
			assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
			assert(MV_IS_STRING(key));
			if ((0 == key->str.len) && (LVNULLSUBS_NEVER == TREF(lv_null_subs)))
			{
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_LVNULLSUBS);
			}
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
			lv = lvAvlTreeLookupStr(lvt, key, &parent);
		} else
		{
			tmp_sbs = *key;
			key = &tmp_sbs;
			MV_FORCE_NUM(key);
			TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(key);	/* used by the lvAvlTreeLookup* functions below */
			if (MVTYPE_IS_INT(tmp_sbs.mvtype))
				lv = lvAvlTreeLookupInt(lvt, key, &parent);
			else
				lv = lvAvlTreeLookupNum(lvt, key, &parent);
		}
	}
	if (!lv || !LV_IS_VAL_DEFINED(lv))
	{
		if (undef_inhibit)
			lv = (lvTreeNode *)&literal_null;
		else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_UNDEF, 2, src->str.len, src->str.addr);
	}
	return (lv_val *)lv;
}
