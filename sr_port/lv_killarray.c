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

void lv_killarray(lv_sbs_tbl *a)
{
       	register lv_val *lv;
	sbs_flt_struct	*f;
	sbs_str_struct	*s;
	int4		i;
	lv_sbs_tbl	*tbl;
	sbs_blk		*blk, *temp;
	char		*top;
	register lv_val **fl;

	assert(a);

	assert(a->sym->ident == MV_SYM);
       	fl = &(a->sym->lv_flist);
	if (a->num)
	{	if (a->int_flag)
		{  	assert(a->num);
			blk = a->num;
 	 	   	for (i = 0; i < SBS_NUM_INT_ELE; i++)
       	       	       	{      	if (lv = blk->ptr.lv[i])
		   	 	{
		   			lv->v.mvtype = 0;
		   			if (lv->ptrs.val_ent.children)
		   			{	lv_killarray(lv->ptrs.val_ent.children);
		   			}
		   			lv->ptrs.free_ent.next_free = *fl;
		   			*fl = lv;
		   		}
	       	   	}
       	       	       	lv_free_sbs_blk(a->num);
		   	a->int_flag = 0;
		}
		else
       	       	{      	for (blk = a->num; blk; )
		 	{	for (f = &blk->ptr.sbs_flt[0], top = (char *)&blk->ptr.sbs_flt[blk->cnt];
					f < (sbs_flt_struct *)top; f++)
		 	 	{	lv = f->lv;
		 			lv->v.mvtype = 0;
		 			if (lv->ptrs.val_ent.children)
		 			{	lv_killarray(lv->ptrs.val_ent.children);
		 			}
		 			lv->ptrs.free_ent.next_free = *fl;
		 			*fl = lv;
		 		}
				temp = blk->nxt;
		 		lv_free_sbs_blk(blk);
		 		blk = temp;
	 	 	}
		}
		a->num = 0;
	}
	if (a->str)
	{      	for (blk = a->str; blk; )
	 	{	for (s = &blk->ptr.sbs_str[0], top = (char *)&blk->ptr.sbs_str[blk->cnt];
				s < (sbs_str_struct *)top; s++)
	 	 	{	lv = s->lv;
	 			lv->v.mvtype = 0;
	 			if (lv->ptrs.val_ent.children)
	 			{	lv_killarray(lv->ptrs.val_ent.children);
	 			}
	 			lv->ptrs.free_ent.next_free = *fl;
	 			*fl = lv;
	 		}
			temp = blk->nxt;
	 		lv_free_sbs_blk(blk);
	 		blk = temp;
	 	}
	 	a->str = 0;
       	}
	/* now free the subscript table */
	lv = (lv_val *)a;
	lv->ptrs.free_ent.next_free = *fl;
	*fl = lv;
	return;
}
