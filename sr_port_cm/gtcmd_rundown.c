/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"

#include "hashtab_mname.h"
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
#include "change_reg.h"
#include "gds_rundown.h"
#include "dpgbldir.h"
#include "wcs_timer_start.h"
#include "gtcmd.h"
#include "send_msg.h"
#include "targ_alloc.h"

GBLREF	cm_region_head		*reglist;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	jnl_gbls_t		jgbl;

#ifdef VMS
GBLREF short		gtcm_ast_avail;
#endif

void gtcmd_rundown(connection_struct *cnx, bool clean_exit)
{
	int4			link;
	cm_region_list		*ptr, *last, *que_next, *que_last;
	cm_region_head		*region;
	uint4			jnl_status;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;

	for (ptr = cnx->region_root;  ptr;)
	{
		region = ptr->reghead;
		TP_CHANGE_REG(region->reg);
		jpc = cs_addrs->jnl;
		if (ptr->pini_addr && clean_exit && JNL_ENABLED(cs_data) && (NOJNL != jpc->channel))
		{
			jpc->pini_addr = ptr->pini_addr;
			grab_crit(gv_cur_region);
			SET_GBL_JREC_TIME; /* jnl_ensure_open/jnl_put_jrt_pfin needs this to be set */
			jbp = jpc->jnl_buff;
			/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl records.
			 * This needs to be done BEFORE the jnl_ensure_open as that could write journal records
			 * (if it decides to switch to a new journal file)
			 */
			ADJUST_GBL_JREC_TIME(jgbl, jbp);
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{
				if (0 != jpc->pini_addr)
					jnl_put_jrt_pfin(cs_addrs);
			} else
				send_msg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
			rel_crit(gv_cur_region);
		}
		if (0 == --region->refcnt)
		{	/* free up only as little as needed to facilitate structure reuse when the region is opened again */
			assert(region->head.fl == region->head.bl);
			VMS_ONLY(gtcm_ast_avail++);
			if (JNL_ALLOWED(cs_data))
				jpc->pini_addr = 0;
			gds_rundown();
			gd_ht_kill(region->reg_hash, TRUE);	/* TRUE to free up the table and the gv_targets it holds too */
			cs_addrs->dir_tree->regcnt--;	/* targ_free relies on this */
			targ_free(cs_addrs->dir_tree);
			cs_addrs->dir_tree = NULL;
			cm_del_gdr_ptr(gv_cur_region);
		} else
			wcs_timer_start(gv_cur_region, TRUE);
		que_next = (cm_region_list *)((unsigned char *)ptr + ptr->regque.fl);
		que_last = (cm_region_list *)((unsigned char *)ptr + ptr->regque.bl);
		link = (int4)((unsigned char *)que_next - (unsigned char *)que_last);
		que_last->regque.fl = link;
		que_next->regque.bl = -link;
		last = ptr;
		ptr = ptr->next;
		free(last);
	}
}
