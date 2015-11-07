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
#include "filestruct.h"
#include "wcs_recover.h"

void bt_que_refresh(gd_region *greg)
{
	sgmnt_addrs *cs;
	bt_rec *que_base, *que_top, *p;
	int 	i;

	cs = &FILE_INFO(greg)->s_addrs;
	i = 0;
	for (que_base = cs->bt_header, que_top = que_base + cs->hdr->bt_buckets + 1; que_base < que_top ; que_base++)
	{
		assert(que_base->blk == BT_QUEHEAD);
		for (p = (bt_rec *) ((char *) que_base + que_base->blkque.fl) ; p != que_base ;
			p = (bt_rec *) ((char *) p + p->blkque.fl))
		{
			if (i++ > cs->hdr->n_bts)
			{	wcs_recover(greg);
				return;
			}
			if ((int4)p & 3)
			{	wcs_recover(greg);
				return;
			}
			p->cache_index = CR_NOTVALID;
		}
	}
	return;
}
