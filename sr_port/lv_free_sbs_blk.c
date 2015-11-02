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

#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "mdq.h"

GBLREF sbs_blk 	*sbs_blk_hdr;
GBLREF int4	lv_sbs_blk_size;

void lv_free_sbs_blk(sbs_blk *b)
{
	dqdel(b, sbs_que);
	DEBUG_ONLY(memset(b, 0, lv_sbs_blk_size));
	b->sbs_que.fl = sbs_blk_hdr;
	sbs_blk_hdr = b;
}
