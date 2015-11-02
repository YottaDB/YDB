/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "dpgbldir.h"
#include "gtmimagename.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_addr		*gd_header;

void gds_map_moved(sm_uc_ptr_t new_base, sm_uc_ptr_t old_base, sm_uc_ptr_t old_top, off_t new_eof)
{
	int		hist_index;
	sm_long_t	adj;
	srch_hist	*hist, *dir_hist, *dir_alt_hist, *hist1, *hist2;
	ht_ent_mname 	*tabent, *topent;
	gv_namehead	*gvt;
	gvnh_reg_t	*gvnh_reg;
	hash_table_mname	*tbl;
	gd_addr		*addr_ptr;

	assert(cs_addrs->now_crit);
	/* It's possible to arrive here via mupip_backup --> wcs_flu --> wcs_mm_recover --> gds_map_moved and have
	 * cs_addrs->dir_tree be NULL.  To distinguish that case, we can also check if this this is the GTM image
	 * in the following assert and then return because there's nothing to do here.
	 */
	assert(!IS_GTM_IMAGE || ((NULL != cs_addrs->dir_tree) && (NULL != &cs_addrs->dir_tree->hist)));
	/* This initialization has to be done irrespective of whether new_base is different from old_base or not. */
	cs_data = cs_addrs->hdr = (sgmnt_data_ptr_t)new_base;
	cs_addrs->db_addrs[1] = new_base + new_eof - 1;
	cs_addrs->bmm = MM_ADDR(cs_data);
	cs_addrs->acc_meth.mm.base_addr = (sm_uc_ptr_t)((sm_uc_ptr_t)cs_data + (cs_data->start_vbn - 1) * DISK_BLOCK_SIZE);
	if (NULL != cs_addrs->sgm_info_ptr)
		cs_addrs->sgm_info_ptr->tp_csd = cs_addrs->hdr;
	bt_init(cs_addrs);
	if (NULL == cs_addrs->dir_tree)
		return;
	/* The following adjustment needs to be done only if new_base is different from old_base */
	if (new_base == old_base)
		return;
	adj = (sm_long_t)(new_base - old_base);
	assert(0 != adj);
	dir_hist = &cs_addrs->dir_tree->hist;
	dir_alt_hist = cs_addrs->dir_tree->alt_hist;
	for (hist = dir_hist; (NULL != hist); hist = (hist == dir_hist) ? dir_alt_hist : NULL)
	{
		for (hist_index = 0;  HIST_TERMINATOR != hist->h[hist_index].blk_num;  hist_index++)
		{
			assert(MAX_BT_DEPTH >= hist_index);
			if ((old_base <= hist->h[hist_index].buffaddr) &&
			    (old_top > hist->h[hist_index].buffaddr))
			{
				hist->h[hist_index].buffaddr += adj;
				assert(new_base <= hist->h[hist_index].buffaddr);
			} else
			{
				/* It has to be a private copy */
				assert((hist->h[hist_index].first_tp_srch_status != 0) ||
				       (((off_chain *)&(hist->h[hist_index].blk_num))->flag != 0));
			}
		}
	}
	/* It is possible that more than one global directory has regions mapping to the same physical database file.
	 * In this case, the search histories in the gv_targets hash tables of all those glds should be fixed.  Hence,
	 * we go through all open glds (instead of just the currently active gd_header).
	 */
	assert(NULL != get_next_gdr(NULL));	/* assert that we have at least one open global directory */
	for (addr_ptr = get_next_gdr(NULL); NULL != addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		tbl = addr_ptr->tab_ptr;
		assert(NULL != tbl);
		for (tabent = tbl->base, topent = tbl->top; tabent < topent; tabent++)
		{
			if ((HTENT_VALID_MNAME(tabent, gvnh_reg_t, gvnh_reg)) && (NULL != (gvt = gvnh_reg->gvt))
				&& (cs_addrs == gvt->gd_csa) && (0 < gvt->clue.end))
			{
				hist1 = &gvt->hist;
				hist2 = gvt->alt_hist;
				for (hist = hist1; (NULL != hist); hist = (hist == hist1) ? hist2 : NULL)
				{
					if (((hist == hist1) && (hist == dir_hist))
							|| ((hist == hist2) && (hist == dir_alt_hist)))
						continue;
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
	}
	return;
}
