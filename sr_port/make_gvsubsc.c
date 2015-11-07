/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mvalconv.h"
#include "collseq.h"

GBLREF spdesc stringpool;

oprtype make_gvsubsc(mval *v)
{
	mval	w;
	gv_key	*gp;

	ENSURE_STP_FREE_SPACE(MAX_SRCLINE + SIZEOF(gv_key));
	if ((INTPTR_T)stringpool.free & 1)	/* BYPASSOK */
		stringpool.free++;	/* word align key for structure refs */
	gp = (gv_key *) stringpool.free;
	gp->top = MAX_SRCLINE;
	gp->end = gp->prev = 0;
	mval2subsc(v, gp, STD_NULL_COLL_FALSE);
	memset(&w, 0, SIZEOF(mval));	/* Clear mval so ununsed fields don't cause objhash differences */
	w.mvtype = MV_STR | MV_SUBLIT;
	w.str.addr = (char *) gp->base;
	w.str.len = gp->end + 1;
	stringpool.free = &gp->base[gp->end + 1];
	assert(stringpool.free <= stringpool.top);
	return put_lit(&w);
}
