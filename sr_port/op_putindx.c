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
#include "gtm_stdio.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
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

GBLDEF lv_val		*active_lv;
GBLREF int		lv_null_subs;
GBLREF collseq		*local_collseq;

lv_val	*op_putindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...)
{
	int	subs_level;
	mval	tmp_sbs;
        int	length;
	va_list	var;
	int4	temp;
	VMS_ONLY(int	argcnt;)
	lv_val	*lv;
 	lv_sbs_tbl *tbl;
 	sbs_blk	*blk;
       	sbs_search_status status;
	mval	*key;
	boolean_t is_canonical;

	error_def(ERR_LVNULLSUBS);

	VAR_START(var, start);
	VMS_ONLY(va_count(argcnt);)
	/* if this variable is marked as a Transaction Processing protected variable, clone the tree. */
	if (NULL != start->tp_var && !start->tp_var->var_cloned)
		TP_VAR_CLONE(start);
	for (subs_level = 1 + ((MV_SBS != start->ptrs.val_ent.parent.sbs->ident) ? 0 : start->ptrs.val_ent.parent.sbs->level);
				--argcnt > 0;  start = lv, subs_level++)
	{
		key = va_arg(var, mval *);
		MV_FORCE_DEFINED(key);	/* Subscripts for set shouldn't be undefined - check here enables lvnullsubs to work */
		lv = NULL;
		if (!(is_canonical = MV_IS_CANONICAL(key)))
		{
			if (!key->str.len && (LVNULLSUBS_OK != lv_null_subs))	/* Error for both LVNULLSUBS_{NO,NEVER} */
			{
				active_lv = start;
				va_end(var);
				rts_error(VARLSTCNT(1) ERR_LVNULLSUBS);
			}
		}
		if (tbl = start->ptrs.val_ent.children)
			assert(MV_SBS == tbl->ident);
		else
		{
			tbl = start->ptrs.val_ent.parent.sbs;
			if (MV_SBS == tbl->ident)
			{
				assert(MV_SYM == tbl->sym->ident);
				start->ptrs.val_ent.children = (lv_sbs_tbl *)lv_getslot(tbl->sym);
				tbl = start->ptrs.val_ent.children;
				memset(tbl, 0, SIZEOF(lv_sbs_tbl));
				tbl->sym = start->ptrs.val_ent.parent.sbs->sym;
			} else
			{	assert(MV_SYM == tbl->ident);
				assert(MV_SYM == start->ptrs.val_ent.parent.sym->ident);
				start->ptrs.val_ent.children = (lv_sbs_tbl *)lv_getslot(start->ptrs.val_ent.parent.sym);
				tbl = start->ptrs.val_ent.children;
				memset(tbl, 0, SIZEOF(lv_sbs_tbl));
				tbl->sym = start->ptrs.val_ent.parent.sym;
			}
			tbl->lv = start;
			tbl->ident = MV_SBS;
		}
		assert(tbl->sym);
		if (!is_canonical)
		{
			if (!(blk = tbl->str))
			{
			      	tbl->str = blk = (sbs_blk *)lv_get_sbs_blk(tbl->sym);
				assert(0 ==blk->cnt);
				assert(0 == blk->nxt);
				assert(blk->sbs_que.fl && blk->sbs_que.bl);
				status.prev = blk;
				status.blk = blk;
				status.ptr = (char *)&blk->ptr.sbs_str[0];
			}
			if (local_collseq)
			{
				ALLOC_XFORM_BUFF(&key->str);
				tmp_sbs.mvtype = MV_STR;
				tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
				assert(NULL != lcl_coll_xform_buff);
				tmp_sbs.str.addr = lcl_coll_xform_buff;
				do_xform(local_collseq, XFORM, &key->str, &tmp_sbs.str, &length);
				tmp_sbs.str.len = length;
				s2pool(&(tmp_sbs.str));
				key = &tmp_sbs;
			}
			if (!(lv = lv_get_str_inx(blk, &key->str, &status)))
				lv = lv_ins_str_sbs(&status, key, tbl);
		} else
		{
			MV_FORCE_NUM(key);
			if (!(blk = tbl->num))
			{
				tbl->num = blk = (sbs_blk *)lv_get_sbs_blk(tbl->sym);
				assert(0 == blk->cnt);
				assert(0 == blk->nxt);
				assert(blk->sbs_que.fl && blk->sbs_que.bl);
				if (MV_IS_INT(key))
				{
					temp = MV_FORCE_INT(key);
					if (temp >= 0 && temp < SBS_NUM_INT_ELE)
					{
						tbl->int_flag = TRUE;
						memset(&blk->ptr, 0, SIZEOF(blk->ptr));
						blk->cnt = 1;
						lv = blk->ptr.lv[temp] = lv_getslot(tbl->sym);
						memset(lv, 0, SIZEOF(lv_val));
						lv->ptrs.val_ent.parent.sbs = tbl;
					}
				}
			} else if (tbl->int_flag)
			{
				if (MV_IS_INT(key) && (temp = MV_FORCE_INT(key)) >= 0 && temp < SBS_NUM_INT_ELE)
				{
					if (!(lv = blk->ptr.lv[temp]))
					{
						lv = blk->ptr.lv[temp] = lv_getslot(tbl->sym);
						memset(lv, 0, SIZEOF(lv_val));
						lv->ptrs.val_ent.parent.sbs = tbl;
						blk->cnt++;
					}
				} else
				{
					lv_cnv_int_tbl(tbl);	/* then do the lv_get_num_inx & lv_ins_num_sbs */
					blk = tbl->num;		/* the conversion frees the old block and gets a new one */
				}
			}
			if (!lv)
			{
				if (!(lv = lv_get_num_inx(blk, key, &status)))
					lv = lv_ins_num_sbs(&status, key, tbl);
			}
		}
		tbl->level = subs_level;
	}
	va_end(var);
	/* This var is about to be set/modified. If it exists and is an alias container var, that reference is going
	   to go away. Take care of that possibility now.
	*/
	if (MV_DEFINED(&lv->v))
	{
		DECR_AC_REF(lv, TRUE);
		lv->v.mvtype &= ~MV_ALIASCONT;	/* Value being replaced is now no longer a container var */
	}
	active_lv = lv;
	return lv;
}
