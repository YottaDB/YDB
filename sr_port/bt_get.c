/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gdsbgtr.h"		/* for the BG_TRACE_PRO macros */

GBLREF sgmnt_addrs	*cs_addrs;

bt_rec_ptr_t bt_get(int4 block)		/* block = block # to get */
{
	register sgmnt_addrs	*csa;
	bt_rec_ptr_t		bt;
	int			lcnt;

	csa = cs_addrs;
	assert(csa->now_crit);
	bt = csa->bt_header + (block % csa->hdr->bt_buckets);
	assert(bt->blk == BT_QUEHEAD);
	for (lcnt = csa->hdr->n_bts; lcnt > 0; lcnt--)
	{
		bt = (bt_rec_ptr_t)((sm_uc_ptr_t) bt + bt->blkque.fl);
		if (bt->blk == block)
			return bt;
		if (bt->blk == BT_QUEHEAD)
			return NULL;
	}
	SET_TRACEABLE_VAR(csa->nl->wc_blocked, TRUE);
	BG_TRACE_PRO_ANY(csa, wc_blocked_bt_get);
	return NULL;	/* actually should return BT_INVALID or some such value but callers check only for NULL */
}
