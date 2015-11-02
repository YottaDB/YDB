/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"         /* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "gds_map_moved.h"
#include "hashtab.h"
#include "hashtab_mname.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

void gds_map_moved(hash_table_mname *tbl, sm_uc_ptr_t new_base, sm_uc_ptr_t old_base, sm_uc_ptr_t old_top)
{
	int		hist_index;
	sm_long_t	adj;
	srch_hist	*hist, *dir_hist;
	ht_ent_mname 	*tabent, *topent;
	gv_namehead	*gvt;

	assert(cs_addrs->now_crit);
	assert((NULL != cs_addrs->dir_tree) && (NULL != &cs_addrs->dir_tree->hist));
	adj = (sm_long_t)(new_base - old_base);
	assert(0 != adj);
	dir_hist = hist = &cs_addrs->dir_tree->hist;
	for (hist_index = 0;  HIST_TERMINATOR != hist->h[hist_index].blk_num;  hist_index++)
	{
		assert(MAX_BT_DEPTH >= hist_index);
		if ((old_base <= hist->h[hist_index].buffaddr) &&
			(old_top > hist->h[hist_index].buffaddr))
		{
			hist->h[hist_index].buffaddr += adj;
			assert(new_base <= hist->h[hist_index].buffaddr);
		}
		else
		{
			/* It has to be a private copy */
			assert((hist->h[hist_index].first_tp_srch_status != 0) ||
				(((off_chain *)&(hist->h[hist_index].blk_num))->flag != 0));
		}
	}

	if (NULL == tbl)
		return;		/* for mupip recover */

	for (tabent = tbl->base, topent = tbl->top; tabent < topent; tabent++)
	{
		if ((HTENT_VALID_MNAME(tabent, gv_namehead, gvt)) && gv_cur_region == gvt->gd_reg && gvt->clue.end > 0)
		{
			hist = &(((gv_namehead *)(tabent->value))->hist);
			if (hist == dir_hist)
				continue;
			for (hist_index = 0;  HIST_TERMINATOR != hist->h[hist_index].blk_num;  hist_index++)
			{
				assert(MAX_BT_DEPTH >= hist_index);
				if ((old_base <= hist->h[hist_index].buffaddr) &&
					(old_top > hist->h[hist_index].buffaddr))
				{
					hist->h[hist_index].buffaddr += adj;
					assert(new_base <= hist->h[hist_index].buffaddr);
				}
				else
				{
					/* It has to be a private copy */
					assert((hist->h[hist_index].first_tp_srch_status != 0) ||
						(((off_chain *)&(hist->h[hist_index].blk_num))->flag != 0));
				}
			}
		}
	}
	return;
}
