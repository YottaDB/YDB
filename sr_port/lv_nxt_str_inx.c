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
#include "mstrcmp.h"

lv_val *lv_nxt_str_inx(sbs_blk *root, mstr *key, sbs_search_status *status)
{
       	sbs_blk	       	*blk, *nxt, *prev;
 	int4		x;
       	sbs_str_struct 	*p, *top;

	assert (root);
       	prev = root;
       	for (blk = root; ; prev = blk, blk = nxt)
       	{
		if (!(nxt = blk->nxt))
		{
			break;
		}
       	       	x = mstrcmp(key, &nxt->ptr.sbs_str[0].str);
       	       	if (x < 0)
		{
			break;
		}
	}

	status->blk = blk;
	status->prev = prev;
       	for (p = (sbs_str_struct*)&blk->ptr.sbs_str[0], top =  (sbs_str_struct*)&blk->ptr.sbs_str[blk->cnt]; p < top; p++)
       	{
		x = mstrcmp(key, &p->str);
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
	 			p = (sbs_str_struct*)&status->blk->ptr.sbs_str[0];
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
		p = (sbs_str_struct*)&status->blk->ptr.sbs_str[0];
		status->ptr = (char*)p;
		return(p->lv);
	}
	else
	{
		status->ptr = (char*)p;
	 	return(0);
	}
}
