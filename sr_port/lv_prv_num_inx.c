/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

#define eb_atmost(u,v)	(numcmp(u,v) <= 0)

mflt *lv_prv_num_inx (sbs_blk *root, mval *key)
{
	sbs_blk		*blk;
	sbs_flt_struct 	*p, *bot;
	mval		tmp ;

	assert (root);
	assert (root->cnt);
	assert (root->ptr.sbs_flt[0].lv);
	MV_ASGN_FLT2MVAL(tmp,root->ptr.sbs_flt[0].flt);
	if (eb_atmost(key, &tmp))
		return 0;
	for (blk = root; ; blk = blk->nxt)
	{
		if (!blk->nxt)
			break;
		MV_ASGN_FLT2MVAL(tmp,blk->nxt->ptr.sbs_flt[0].flt);
		if (eb_atmost(key, &tmp))
			break;
	}
	bot = blk->ptr.sbs_flt ;
	p = &blk->ptr.sbs_flt[blk->cnt - 1];
	while (MV_ASGN_FLT2MVAL(tmp,p->flt) , eb_atmost(key, &tmp))
	{
		--p;
		if (p < bot)
			return 0;
	}
	return &p->flt;
}
