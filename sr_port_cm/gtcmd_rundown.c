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

#include "cmidef.h"
#include "cmmdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"

GBLREF cm_region_head		*reglist;
GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF short			gtcm_ast_avail;
GBLREF jnl_process_vector	*prc_vec;

void gtcmd_rundown(connection_struct *cnx, bool clean_exit)
{
	int4		link;
	cm_region_list	*ptr, *last, *que_next, *que_last;
	cm_region_head	*region;
	ht_entry	*h, *htop;

	for (ptr = cnx->region_root;  ptr;)
	{
		region = ptr->reghead;
		gv_cur_region = region->reg;
		change_reg();
		if ((ptr->pini_addr && clean_exit) && (TRUE == JNL_ENABLED(cs_addrs->hdr)) && (NOJNL != cs_addrs->jnl->channel))
		{
			cs_addrs->jnl->pini_addr = ptr->pini_addr;
			prc_vec = cnx->pvec;
			grab_crit(gv_cur_region);
			jnl_put_jrt_pfin(cs_addrs);
			rel_crit(gv_cur_region);
		}
		if (0 == --region->refcnt)
		{	/* free up only as little as needed to facilitate structure reuse when the region is opened again */
			assert(region->head.fl == region->head.bl);
			gtcm_ast_avail++;
			if (JNL_ALLOWED(cs_addrs->hdr))
				cs_addrs->jnl->pini_addr = 0;
			prc_vec = cnx->pvec;
			assert(NULL != prc_vec);
			gds_rundown();
			for (h = ((htab_desc *)(region->reg_hash))->base, htop = h + ((htab_desc *)(region->reg_hash))->size;
				h < htop;  h++)
			{
				if (h->ptr)
				{
					free(((gv_namehead *)h->ptr)->alt_hist);
					free(h->ptr);
				}
			}
			free(((htab_desc*)(region->reg_hash))->base);
			free(FILE_INFO(gv_cur_region)->s_addrs.dir_tree->alt_hist);
			free(FILE_INFO(gv_cur_region)->s_addrs.dir_tree);
			cm_del_gdr_ptr(gv_cur_region);
		} else
			wcs_timer_start(gv_cur_region, TRUE);
		que_next = (cm_region_list *)((unsigned char *)ptr + ptr->regque.fl);
		que_last = (cm_region_list *)((unsigned char *)ptr + ptr->regque.bl);
		link = (unsigned char *)que_next - (unsigned char *)que_last;
		que_last->regque.fl = link;
		que_next->regque.bl = -link;
		last = ptr;
		ptr = ptr->next;
		free(last);
	}
}
