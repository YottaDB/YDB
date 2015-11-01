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

mstr *lv_prv_str_inx (sbs_blk *root, mstr *key)
{
       	sbs_blk	       	*blk;
       	sbs_str_struct 	*p, *bot;

	assert (root);
	assert (root->cnt);
	assert (root->ptr.sbs_str[0].lv);
	if (mstrcmp (key, &root->ptr.sbs_str[0].str) <= 0)
		return 0;
	for (blk = root; ; blk = blk->nxt)
		if (!blk->nxt || (mstrcmp (key, &blk->nxt->ptr.sbs_str[0].str) <= 0))
			break;
	for (bot = blk->ptr.sbs_str, p = &blk->ptr.sbs_str[blk->cnt - 1]; bot <= p && mstrcmp (key, &p->str) <= 0; --p)
		;
	if (p < bot)
		return 0;
	else
		return &p->str;
}
