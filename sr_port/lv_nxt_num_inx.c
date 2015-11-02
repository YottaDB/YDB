/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "numcmp.h"

lv_val *lv_nxt_num_inx(sbs_blk *root, mval *key, sbs_search_status *status)
{
       	sbs_blk	       	*blk, *nxt, *prev;
       	sbs_flt_struct 	*p, *top;
	mval		tmp;
 	long		x;

	assert(root);
       	prev = root;
       	for (blk = root; ; prev = blk, blk = nxt)
       	{
		if (!(nxt = blk->nxt))
		{
			break;
	 	}
		MV_ASGN_FLT2MVAL(tmp,nxt->ptr.sbs_flt[0].flt);
		if (key->mvtype & MV_INT & tmp.mvtype)
			x = key->m[1] - tmp.m[1];
		else
	       	       	x = numcmp(key,&tmp) ;
       	       	if (x < 0)
		{
			break;
		}
	}

	status->blk = blk;
	status->prev = prev;
       	for (p = (sbs_flt_struct*)&blk->ptr.sbs_flt[0], top =  (sbs_flt_struct*)&blk->ptr.sbs_flt[blk->cnt]; p < top; p++)
       	{
		MV_ASGN_FLT2MVAL(tmp,p->flt);
		if (key->mvtype & MV_INT & tmp.mvtype)
			x = key->m[1] - tmp.m[1];
		else
	       	       	x = numcmp(key,&tmp) ;
       	 	if (x == 0)
       	       	{
			if (++p < top)
		 	{
				status->ptr = (char*)p;
				return(p->lv);
			}
			else if (blk->nxt)
			{
				status->prev = blk;
				status->blk  = blk->nxt;
				assert(status->blk->cnt);
       	       	       	       	p = (sbs_flt_struct*)&status->blk->ptr.sbs_flt[0];
				status->ptr = (char*)p;
				return(p->lv);
			}
	 	}
	 	if (x < 0)
       	       	{
			status->ptr = (char*)p;
			return(p->lv);
	 	}
	}
	if (blk->nxt)
	{
		status->prev = blk;
		status->blk  = blk->nxt;
		assert(status->blk->cnt);
       	       	p = (sbs_flt_struct*)&status->blk->ptr.sbs_flt[0];
		status->ptr = (char*)p;
		return(p->lv);
	}
	else
	{
	       	status->ptr = (char*)p;
		return(0);
	}
}
