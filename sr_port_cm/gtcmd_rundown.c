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

error_def(ERR_NOTALLDBRNDWN);

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
	int			refcnt;
	boolean_t		was_crit;
	int4			rundown_status = EXIT_NRM;			/* if gds_rundown went smoothly */

	for (ptr = cnx->region_root;  ptr;)
	{
		region = ptr->reghead;
		TP_CHANGE_REG(region->reg);
		jpc = cs_addrs->jnl;
		if (ptr->pini_addr && clean_exit && JNL_ENABLED(cs_data) && (NOJNL != jpc->channel))
		{
			was_crit = cs_addrs->now_crit;
			if (!was_crit)
				grab_crit(gv_cur_region);
			if (JNL_ENABLED(cs_data))
			{
				jpc->pini_addr = ptr->pini_addr;
				SET_GBL_JREC_TIME; /* jnl_ensure_open/jnl_put_jrt_pfin needs this to be set */
				jbp = jpc->jnl_buff;
				/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order
				 * of jnl records.  This needs to be done BEFORE the jnl_ensure_open as that could write
				 * journal records (if it decides to switch to a new journal file).
				 */
				ADJUST_GBL_JREC_TIME(jgbl, jbp);
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
				{
					if (0 != jpc->pini_addr)
						jnl_put_jrt_pfin(cs_addrs);
				} else
					send_msg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
			}
			if (!was_crit)
				rel_crit(gv_cur_region);
		}
		refcnt = --region->refcnt;
		/* Dont know how refcnt can become negative but in pro handle it by bypassing this region. The reason is the
		 * following. refcnt should have originally been a positive value. Every time this function is invoked, it would
		 * be decremented by one. There should have been one invocation that saw refcnt to be zero. That would have
		 * done the rundown of the region or if it is still in the stack the rundown is still in progress. Therefore
		 * it is not a good idea to try running down this region when we see refcnt to be negative (as otherwise we
		 * will get confused and could potentially end up with SIG-11 or ACCVIO errors). The worst case is that we
		 * would not have rundown the region in which case an externally issued MUPIP RUNDOWN would be enough.
		 */
		assert(0 <= refcnt);
		if (0 == refcnt)
		{	/* free up only as little as needed to facilitate structure reuse when the region is opened again */
			assert(region->head.fl == region->head.bl);
			VMS_ONLY(gtcm_ast_avail++);
			if (JNL_ALLOWED(cs_data))
				jpc->pini_addr = 0;
			UNIX_ONLY(rundown_status |=) gds_rundown();
			gd_ht_kill(region->reg_hash, TRUE);	/* TRUE to free up the table and the gv_targets it holds too */
			FREE_CSA_DIR_TREE(cs_addrs);
			cm_del_gdr_ptr(gv_cur_region);
		}
		que_next = (cm_region_list *)((unsigned char *)ptr + ptr->regque.fl);
		que_last = (cm_region_list *)((unsigned char *)ptr + ptr->regque.bl);
		link = (int4)((unsigned char *)que_next - (unsigned char *)que_last);
		que_last->regque.fl = link;
		que_next->regque.bl = -link;
		last = ptr;
		ptr = ptr->next;
		free(last);
	}

	if (EXIT_NRM != rundown_status)
		rts_error(VARLSTCNT(1) ERR_NOTALLDBRNDWN);
}
