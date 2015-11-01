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
#include "mstrcmp.h"

lv_val *lv_get_str_inx(sbs_blk *root, mstr *key, sbs_search_status *status)
{
       	sbs_blk	       	*blk, *nxt, *prev;
 	int4		x;
       	sbs_str_struct 	*p, *top;

	assert (root);
       	prev = root;
       	for (blk = root; ; prev = blk, blk = nxt)
       	{      	if (!(nxt = blk->nxt))
		{	break;
		}
       	       	x = mstrcmp(key, &nxt->ptr.sbs_str[0].str);
		if (x < 0)
		{	break;
		}
       	       	if (x == 0)
       	       	{      	status->blk = nxt;
			status->prev = blk;
			status->ptr = (char*)&nxt->ptr.sbs_str[0];
			return(nxt->ptr.sbs_str[0].lv);
		}
	}

	status->blk = blk;
	status->prev = prev;
       	for (p = (sbs_str_struct*)&blk->ptr.sbs_str[0], top =  (sbs_str_struct*)&blk->ptr.sbs_str[blk->cnt]; p < top; p++)
       	{      	x = mstrcmp(key, &p->str);
	 	if (x == 0)
       	       	{      	status->ptr = (char*)p;
			return(p->lv);
	 	}
	 	if (x < 0)
	 	{	break;
	 	}
	}
	status->ptr = (char*)p;
	return(0);
}
