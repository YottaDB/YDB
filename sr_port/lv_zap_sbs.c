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

void lv_zap_sbs(lv_sbs_tbl *tbl, lv_val *lv)
{
	sbs_blk		*blk, *prev;
	char		hit, *top;
	sbs_str_struct	*s, *s1;
	sbs_flt_struct	*f, *f1;
	lv_val		**lvp, **free, *lv1;
	lv_sbs_tbl	*ptbl;

	hit = 0;
	assert(tbl->ident == MV_SBS);
	assert(tbl->sym->ident == MV_SYM);
	if (tbl->str)
	{
		for (prev = blk = tbl->str; blk; prev = blk, blk = blk->nxt)
		{	for (s = &blk->ptr.sbs_str[0], top = (char *)&blk->ptr.sbs_str[blk->cnt];
				s < (sbs_str_struct *)top; s++)
			{	if (s->lv == lv)
		 		{	hit = 1;
					for (s1 = s + 1; s1 < (sbs_str_struct *)top; s1++, s++)
		 			{	*s = *s1;
		 			}
					blk->cnt--;
					if (blk->cnt == 0)
					{	if (prev == blk)
						{	tbl->str = blk->nxt;
						}
						else
						{	assert(prev->nxt == blk);
							prev->nxt = blk->nxt;
		     				}
						lv_free_sbs_blk(blk);
					}
				 	break;
		 		}
			}
			if (hit)
			{	break;
			}
	    	}
	}
	if (!hit && tbl->num)
	{
		if (tbl->int_flag)
		{	blk = tbl->num;
			for (lvp = &blk->ptr.lv[0], top = (char *)&blk->ptr.lv[SBS_NUM_INT_ELE];
				lvp < (lv_val **)top; lvp++)
			{	if (*lvp == lv)
				{	*lvp = 0;
					hit = 1;
					blk->cnt--;
					if (blk->cnt == 0)
					{
						tbl->int_flag = FALSE;
						tbl->num = 0;
						lv_free_sbs_blk(blk);
					}
					break;
				}
			}
		}
		else
		{
			for (prev = blk = tbl->num; blk; prev = blk, blk = blk->nxt)
			{	for (f = &blk->ptr.sbs_flt[0], top = (char *)&blk->ptr.sbs_flt[blk->cnt];
					f < (sbs_flt_struct *)top; f++)
				{	if (f->lv == lv)
			 		{    	hit = 1;
					       	for (f1 = f + 1; f1 < (sbs_flt_struct *)top; f1++, f++)
			 			{	*f = *f1;
			 			}
						blk->cnt--;
						if (blk->cnt == 0)
						{	if (prev == blk)
						       	{      	tbl->num = blk->nxt;
						 	}
						 	else
						 	{	assert(prev->nxt == blk);
						 	 	prev->nxt = blk->nxt;
			     			 	}
						 	lv_free_sbs_blk(blk);
						}
					 	break;
			 		}
				}
				if (hit)
				{	break;
				}
	       	       	}
		}
	}
	assert(hit);
	if (!tbl->num && !tbl->str)
	{	/* now free the subscript table */
		assert(tbl->sym->ident == MV_SYM);
		tbl->lv->ptrs.val_ent.children = 0;

		if (tbl->lv->v.mvtype == 0)
		{
	       		ptbl = tbl->lv->ptrs.val_ent.parent.sbs;
			if (ptbl->ident == MV_SBS)
			{	assert(tbl->sym->ident == MV_SYM);
				lv1 = tbl->lv;
				lv1->ptrs.free_ent.next_free = tbl->sym->lv_flist;
			 	tbl->sym->lv_flist = tbl->lv;
		 		lv_zap_sbs(ptbl, tbl->lv);
			}
		}
		/* Warning Will Robinson!! tbl->sym and ((lv_val*)tbl)->ptrs.free_ent are the same location.
		 * if you don't have a temp variable the snake will eat its tail.
		 */
		free = &tbl->sym->lv_flist;
		((lv_val*)tbl)->ptrs.free_ent.next_free = *free;
	 	*free = (lv_val*)tbl;
	}
}

