/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "mdq.h"

GBLREF sbs_blk 	*sbs_blk_hdr;

void lv_free_sbs_blk(sbs_blk *b)
{
	dqdel (b, sbs_que);

#ifdef DEBUG
	memset(b,0, SIZEOF(sbs_blk));
#endif

	b->sbs_que.fl = sbs_blk_hdr;
	sbs_blk_hdr = b;
}
