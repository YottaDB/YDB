/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "hashtab_mname.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gv_namehead             *gv_target_list;


void gds_map_moved(sm_uc_ptr_t new_base, sm_uc_ptr_t old_base, sm_uc_ptr_t old_top, size_t mmap_sz)
{
	int		hist_index;
	sm_long_t	adj;
	srch_hist	*hist, *hist1, *hist2;
	gv_namehead	*gvt;
	sgmnt_addrs	*csa;

	csa = cs_addrs;
	assert(csa->now_crit);
	assert(cs_data == csa->hdr);
	assert((NULL == csa->sgm_info_ptr) || (csa->hdr == csa->sgm_info_ptr->tp_csd));
	assert(csa->bmm == MM_ADDR(cs_data));
	assert(csa->ti == &cs_data->trans_hist);
	csa->db_addrs[1] = new_base + mmap_sz - 1;
	/* The following adjustment needs to be done only if new_base is different from old_base */
	if (new_base == old_base)
		return;
	adj = (sm_long_t)(new_base - old_base);
	assert(0 != adj);
	/* Scan the list of gvts allocated by this process in its lifetime and see if they map to the current csa.
	 * If so adjust their clues (in the histories) as appropriate to reflect the remapping of the database file.
	 */
	for (gvt = gv_target_list; NULL != gvt; gvt = gvt->next_gvnh)
	{
		if ((csa == gvt->gd_csa) && (0 < gvt->clue.end))
		{
			hist1 = &gvt->hist;
			hist2 = gvt->alt_hist;
			for (hist = hist1; (NULL != hist); hist = (hist == hist1) ? hist2 : NULL)
			{
				for (hist_index = 0;  HIST_TERMINATOR != hist->h[hist_index].blk_num;  hist_index++)
				{
					assert(MAX_BT_DEPTH >= hist_index);
					if ((old_base <= hist->h[hist_index].buffaddr)
						&& (old_top > hist->h[hist_index].buffaddr))
					{
						hist->h[hist_index].buffaddr += adj;
						assert(new_base <= hist->h[hist_index].buffaddr);
					} else if ((hist == hist2) && (0 < gvt->clue.end))
					{	/* alt_hist is not updated when clue is set so the buffaddr can
						 * point to a prior instance of the file's mapping.  So, reset alt_hist.
						 */
						hist->h[hist_index].blk_num = HIST_TERMINATOR;
					} else
					{	/* It's already been adjusted or it has to be a private copy */
						assert(((new_base <= hist->h[hist_index].buffaddr)
							&& (hist->h[hist_index].buffaddr < new_base + (old_top - old_base)))
							|| (0 != hist->h[hist_index].first_tp_srch_status)
							|| (0 != ((off_chain *)&(hist->h[hist_index].blk_num))->flag));
					}
				}
			}
		}
	}
	return;
}
