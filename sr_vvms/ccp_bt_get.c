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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"


bt_rec *ccp_bt_get(cs_addrs, block)
/* this function returns a pointer to the bt_rec entry or 0 if not found */
int4 block;	/* block number to put */
sgmnt_addrs *cs_addrs;
{
	register sgmnt_addrs *cs;
	bt_rec	*p;

	cs = cs_addrs;
	p = cs->bt_header + (block % cs->hdr->bt_buckets);
	assert(p->blk == BT_QUEHEAD);
	for (;;)
	{	p = (bt_rec *) ((char *) p + p->blkque.fl);
		if (p->blk == block)
		{	return p;
		}
		if (p->blk == BT_QUEHEAD)
		{	return 0;
		}
	}
}
