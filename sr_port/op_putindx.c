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
#include <stddef.h> 		/* for offsetof */
#include "gtm_string.h"
#include "gtm_stdio.h"

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
#include <rtnhdr.h>
#include "toktyp.h"
#include "valid_mname.h"
#include "stack_frame.h"
#ifdef DEBUG
#include "gtm_ctype.h"
#endif

GBLDEF lv_val		*active_lv;

GBLREF symval		*curr_symval;
GBLREF stack_frame	*frame_pointer;
GBLREF bool		undef_inhibit;

error_def(ERR_LVNULLSUBS);
error_def(ERR_MAXSTRLEN);
error_def(ERR_UNDEF);

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
	for (subs_level++; --argcnt > 0; subs_level++)
	{
		key = va_arg(var, mval *);
		MV_FORCE_DEFINED(key);	/* Subscripts for set shouldn't be undefined - check here enables lvnullsubs to work */
		if (!(is_canonical = MV_IS_CANONICAL(key)))
		{
			assert(MV_IS_STRING(key));
			assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
			if (!key->str.len)
			{
				if (LVNULLSUBS_OK != TREF(lv_null_subs))	/* Error for both LVNULLSUBS_{NO,NEVER} */
				{
					active_lv = lv;
					va_end(var);
					rts_error(VARLSTCNT(1) ERR_LVNULLSUBS);
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
			if (lvt = LV_GET_CHILD(lv))	/* caution: assignment */
				assert(MV_LV_TREE == lvt->ident);
			else	/* No children exist at this level - create a child */
				LV_TREE_CREATE(lvt, (lvTreeNode *)lv, subs_level, base_lv);
			lv = (lv_val *)lvAvlTreeLookupStr(lvt, key, &parent);
		} else
		{	/* Need to set canonical bit before calling tree search functions.
			 * But input mval could be read-only so cannot modify that even if temporarily.
			 * So take a copy of the mval and modify that instead.
			 */
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
			if (lvt = LV_GET_CHILD(lv))	/* caution: assignment */
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
		}
	}
	va_end(var);
	/* This var is about to be set/modified. If it exists and is an alias container var,
	 * that reference is going to go away. Take care of that possibility now.
	 */
	if (LV_IS_VAL_DEFINED(lv))
	{
		DECR_AC_REF(lv, TRUE);
		lv->v.mvtype &= ~MV_ALIASCONT;	/* Value being replaced is now no longer a container var */
	}
	active_lv = lv;
	return lv;
}

/* this tucks all the information needed to reaccess a subscripted FOR control variable into a single mval
 * so that op_rfrshindx can later either replace the control variable or determine it was ditched in the scope of the loop
 * the saved information hangs off the stack because the compiler doesn't seem to have a way (assembly modules possible
 * excepted) to manage an mval pointer and no where else provides a stable enough anchor
 */
lv_val	*op_savputindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...)
{	/* this saves the arguments as statics and then does a putindx */
	char			*c, *ptr;
	lvname_info_ptr		lvn_info;
	lv_val			*lv;
	mname_entry		*targ_key;
	mval			*key, *saved_indx;
	uint4			subs_level, total_len;
	va_list			var;
	VMS_ONLY(int		argcnt;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, start);
	VMS_ONLY(va_count(argcnt);)
	assert(0 < argcnt);
	assert(NULL != start);
	total_len = SIZEOF(mval) + SIZEOF(mname_entry) + SIZEOF(mident_fixed) + SIZEOF(lvname_info);
	total_len += (SIZEOF(mval) * (argcnt - 1));
	saved_indx = (mval *)malloc(total_len);
	ptr = (char *)saved_indx + SIZEOF(mval);
	saved_indx->mvtype = MV_STR;
	saved_indx->str.addr = (char *)ptr;
	saved_indx->str.len = total_len - SIZEOF(mval);
	targ_key = (mname_entry *)ptr;
	ptr += SIZEOF(mname_entry);
	targ_key->var_name.addr = (char *)ptr;
	ptr += SIZEOF(mident_fixed);
	lvn_info = (lvname_info_ptr)ptr;
	ptr += SIZEOF(lvname_info);
	lvn_info->total_lv_subs = argcnt--;
	assert((0 <= argcnt) && (MAX_FORARGS > argcnt));
	lvn_info->start_lvp = start;
	for (subs_level = 0; subs_level < argcnt; subs_level++)
	{
		key = va_arg(var, mval *);
		lvn_info->lv_subs[subs_level] = (mval *)ptr;
		*(mval *)ptr = *(mval *)key;
		ptr += SIZEOF(mval);
	}
	assert((char *)saved_indx + total_len >= ptr);
	va_end(var);
	lv = (lv_val *)callg((INTPTR_T (*)(intszofptr_t argcnt_arg, ...))op_putindx, (gparam_list *)lvn_info);
	c = (char *)format_lvname(start, (unsigned char *)targ_key->var_name.addr, SIZEOF(mident_fixed));
	assert((c < ptr) && (c > (char *)targ_key->var_name.addr));
	targ_key->var_name.len = c - targ_key->var_name.addr;
	COMPUTE_HASH_MNAME(targ_key)
	targ_key->marked = FALSE;
	MANAGE_FOR_INDX(frame_pointer, TREF(for_nest_level), saved_indx);
	return lv;
}
/* this fishs out values saved by op_savputindx or op_indlvaddr, looks up the variable using add_hashtab_mname_symval and calls
 * op_putindx or op_srchtindx as appropriate protecting the long-lived FOR control variable from subsequent actons on the line
 */
lv_val	*op_rfrshindx(uint4 level, boolean_t put)
{
	boolean_t		added;
	ht_ent_mname		*tabent;
	lvname_info_ptr 	lvn_info;
	lv_val			*lv;
	mname_entry		*targ_key;
	mval			*saved_indx;
	unsigned char		buff[512], *end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	saved_indx = frame_pointer->for_ctrl_stack->saved_for_indx[level];
	assert(MV_STR & saved_indx->mvtype);
	targ_key = (mname_entry *)saved_indx->str.addr;
	assert((mname_entry *)((char *)saved_indx + SIZEOF(mval)) == targ_key);
	assert((char *)targ_key + SIZEOF(mname_entry) == targ_key->var_name.addr);
	assert(SIZEOF(mident_fixed) > targ_key->var_name.len);
	added = add_hashtab_mname_symval(&curr_symval->h_symtab, targ_key, NULL, &tabent);
	lvn_info = (lvname_info_ptr)((char *)targ_key + SIZEOF(mname_entry) + SIZEOF(mident_fixed));
	if ((saved_indx->str.addr + saved_indx->str.len) < (char *)lvn_info)
	{	/* Only a name */
		assert((SIZEOF(mname_entry) + ((mname_entry *)(saved_indx->str.addr))->var_name.len) == saved_indx->str.len);
		if (put || MV_DEFINED((mval *)tabent->value))
			return (lv_val *)tabent->value;
		rts_error(VARLSTCNT(4) ERR_UNDEF, 2, targ_key->var_name.len, targ_key->var_name.addr);
	}
	lvn_info->start_lvp = (lv_val *)tabent->value;
	lv = (added && !put) ? NULL : (lv_val *)callg((INTPTR_T (*)(intszofptr_t argcnt_arg, ...))(put ? op_putindx : op_srchindx),
				(gparam_list *)lvn_info);
	assert(NULL != lv || !put);
	if ((NULL != lv) && (put || LV_IS_VAL_DEFINED(lv)))
		return lv;
	end = format_key_mvals(buff, SIZEOF(buff), lvn_info);
	rts_error(VARLSTCNT(4) ERR_UNDEF, 2, end - buff, buff);
	assert(FALSE);	/* should not come back from rts error */
	return lv;		/* make some compilers happy */
}
