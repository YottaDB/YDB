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

/* This function returns a pointer to the bt_rec entry or 0 if not found. */

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

GBLREF sgmnt_addrs	*cs_addrs;

bt_rec_ptr_t bt_get(int4 block)		/* block = block # to put */
{
	register sgmnt_addrs *cs;
	bt_rec_ptr_t	p;

	cs = cs_addrs;
	assert(cs->read_lock || cs->now_crit);
	p = cs->bt_header + (block % cs->hdr->bt_buckets);
	assert(p->blk == BT_QUEHEAD);
	for (;;)
	{
		p = (bt_rec_ptr_t) ((sm_uc_ptr_t) p + p->blkque.fl);
		if (p->blk == block)
		{
			return p;
		}
		else if (p->blk == BT_QUEHEAD)
		{
			return 0;
		}
	}
}
