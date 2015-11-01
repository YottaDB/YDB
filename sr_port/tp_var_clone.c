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

void tp_var_clone(lv_val *var)
{	lv_val		*clone_var, *lv;
	sbs_blk		**num, **str, *newsbs;
	lv_sbs_tbl	*tbl, *newtbl;
	int		i;

	clone_var = var->tp_var ? var->tp_var : var;
	if (tbl = var->ptrs.val_ent.children)
	{
		assert(MV_SBS == tbl->ident);
		assert(MV_SYM == tbl->sym->ident);
		clone_var->ptrs.val_ent.children = (lv_sbs_tbl *)lv_getslot(tbl->sym);
		newtbl = clone_var->ptrs.val_ent.children;
		memcpy(newtbl, tbl, sizeof(lv_sbs_tbl));
		newtbl->lv = clone_var;
		num = &newtbl->num;
		str = &newtbl->str;
		if (*num)
		{
			if (newtbl->int_flag)
			{
				assert(!(*num)->nxt);
				newsbs = (sbs_blk *)lv_get_sbs_blk(newtbl->sym);
				assert(0 == newsbs->cnt);
				assert(0 == newsbs->nxt);
				assert(newsbs->sbs_que.fl && newsbs->sbs_que.bl);
				newsbs->cnt = (*num)->cnt;
				memcpy(&newsbs->ptr, &(*num)->ptr, sizeof(newsbs->ptr));
				for (i = 0;  i < SBS_NUM_INT_ELE;  i++)
				{
					if (newsbs->ptr.lv[i])
					{
						lv = lv_getslot(tbl->sym);
						*lv = *newsbs->ptr.lv[i];
						lv->ptrs.val_ent.parent.sbs = newtbl;
						newsbs->ptr.lv[i] = lv;
						if (lv->ptrs.val_ent.children)
							tp_var_clone(lv);
					}
				}
				*num = newsbs;
			} else
			{
				while (*num)
				{
					newsbs = (sbs_blk *)lv_get_sbs_blk(newtbl->sym);
					assert(0 == newsbs->cnt);
					assert(0 == newsbs->nxt);
					assert(newsbs->sbs_que.fl && newsbs->sbs_que.bl);
					newsbs->cnt = (*num)->cnt;
					memcpy(&newsbs->ptr, &(*num)->ptr, sizeof(newsbs->ptr));
					for (i = 0;  i < newsbs->cnt;  i++)
					{
						lv = lv_getslot(tbl->sym);
						*lv = *newsbs->ptr.sbs_flt[i].lv;
						lv->ptrs.val_ent.parent.sbs = newtbl;
						newsbs->ptr.sbs_flt[i].lv = lv;
						if (lv->ptrs.val_ent.children)
							tp_var_clone(lv);
					}
					newsbs->nxt = (*num)->nxt;
					*num = newsbs;
					num = &newsbs->nxt;
				}
			}
		}
		while (*str)
		{
			newsbs = (sbs_blk *)lv_get_sbs_blk(newtbl->sym);
			assert(0 == newsbs->cnt);
			assert(0 == newsbs->nxt);
			assert(newsbs->sbs_que.fl && newsbs->sbs_que.bl);
			newsbs->cnt = (*str)->cnt;
			memcpy(&newsbs->ptr, &(*str)->ptr, sizeof(newsbs->ptr));
			for (i = 0;  i < newsbs->cnt;  i++)
			{
				lv = lv_getslot(tbl->sym);
				*lv = *newsbs->ptr.sbs_str[i].lv;
				lv->ptrs.val_ent.parent.sbs = newtbl;
				newsbs->ptr.sbs_str[i].lv = lv;
				if (lv->ptrs.val_ent.children)
					tp_var_clone(lv);
			}
			newsbs->nxt = (*str)->nxt;
			*str = newsbs;
			str = &newsbs->nxt;
		}
	}
	/* in order to do restarts of sub-transactions this should follow a chain that is currently not built by op_tstart */
	var->tp_var = NULL;
}
