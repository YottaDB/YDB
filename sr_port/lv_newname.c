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
#include "tp_frame.h"

GBLREF tp_frame *tp_pointer;

void lv_newname(ht_entry *hte, symval *sym)
{
	lv_val	*var, *lv;
	tp_frame *tf;
	tp_var	*restore_ent;
	unsigned char all_cnt, sav_cnt;

	lv = lv_getslot(sym);
	lv->v.mvtype = 0;
	lv->tp_var = NULL;
	lv->ptrs.val_ent.children = 0;
	lv->ptrs.val_ent.parent.sym = sym;
	hte->ptr = (char *)lv;
	if (sym->tp_save_all)
	{
		all_cnt = 0;
		for (tf = tp_pointer; tf; tf = tf->old_tp_frame)
		{
			if (tf->sym)
			{
				if (tf->sym != sym)
					break;
				all_cnt++;
			}
		}
		assert(all_cnt);
		sav_cnt = all_cnt;
		for (tf = tp_pointer; all_cnt > 0; tf = tf->old_tp_frame)
		{
			assert(tf);
			if (tf->sym)
			{
				var = lv_getslot(lv->ptrs.val_ent.parent.sym);
				restore_ent = (tp_var *)malloc(sizeof(*restore_ent));
				restore_ent->current_value = lv;
				restore_ent->save_value = var;
				restore_ent->next = tf->vars;
				tf->vars = restore_ent;
				*var = *lv;
				all_cnt--;
			}
		}
	}
}
