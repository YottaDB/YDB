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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_rtsib.h"
#include "gvcst_search.h"
#include "gvcst_search_blk.h"
#include "muextr.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sm_uc_ptr_t	mu_extr_buffer;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;

int mu_extr_getblk(unsigned char *ptr)
{
	error_def(ERR_GVGETFAIL);
	enum cdb_sc	status;
	rec_hdr_ptr_t	rp;
	bool		two_histories, end_of_tree;
	blk_hdr_ptr_t	bp;
	srch_blk_status	*bh;
	srch_hist	*rt_history;

	t_begin(ERR_GVGETFAIL, FALSE);
	for (;;)
	{
		if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
		{
			t_retry(status);
			continue;
		}
		end_of_tree = two_histories
			    = FALSE;
		bh = gv_target->hist.h;
		rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
		bp = (blk_hdr_ptr_t)bh->buffaddr;
		if (rp >= (rec_hdr_ptr_t)CST_TOB(bp))
		{
			rt_history = gv_target->alt_hist;
			if (cdb_sc_normal == (status = gvcst_rtsib(rt_history, 0)))
			{
				two_histories = TRUE;
				if (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, rt_history->h)))
				{
					t_retry(status);
					continue;
				}
				bp = (blk_hdr_ptr_t)rt_history->h[0].buffaddr;
			} else if (cdb_sc_endtree == status)
					end_of_tree = TRUE;
			else
			{
				t_retry(status);
				continue;
			}
		}
		memcpy(ptr, bp, bp->bsiz);
		if (t_end(&gv_target->hist, two_histories ? rt_history : NULL) != 0)
		{
			if (two_histories)
				memcpy(gv_target->hist.h, rt_history->h, sizeof(srch_blk_status) * (rt_history->depth + 1));
			return !end_of_tree;
		}
	}
}
