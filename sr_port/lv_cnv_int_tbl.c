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
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"

void lv_cnv_int_tbl(lv_sbs_tbl *tbl)
{
       	sbs_blk		*old, *blk;
	sbs_flt_struct	*f;
	int4		i;
       	lv_val 	       	**p, **p_top;

	assert(tbl->int_flag);
       	tbl->int_flag = FALSE;

	assert (tbl->sym);
	old = tbl->num;
	tbl->num = blk = lv_get_sbs_blk(tbl->sym);
	assert (blk->cnt == 0);
	assert (blk->nxt == 0);
	assert (blk->sbs_que.fl && blk->sbs_que.bl);
       	f = (sbs_flt_struct *)&tbl->num->ptr.sbs_flt[0];
       	for (i = 0, p = &old->ptr.lv[0], p_top = (p + SBS_NUM_INT_ELE); p < p_top; p++, i++)
       	{
		if (*p)
       	       	{
			if (blk->cnt >= SBS_NUM_FLT_ELE)
		       	{
				blk->nxt = lv_get_sbs_blk(tbl->sym);
       	       	       	       	blk = blk->nxt;
				assert (blk->cnt == 0);
				assert (blk->nxt == 0);
				assert (blk->sbs_que.fl && blk->sbs_que.bl);
       	       	       	       	f = (sbs_flt_struct *)&blk->ptr.sbs_flt[0];
		       	}

			blk->cnt++;
			MV_FORCE_FLT(&(f->flt),i) ;
       	       	       	f->lv = *p;
			f->lv->ptrs.val_ent.parent.sbs = tbl;
			f++;
	       	}
	}
	lv_free_sbs_blk(old);
}
