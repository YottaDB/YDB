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

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "min_max.h"
#include "tp_set_sgm.h"
#ifdef GTM_TRIGGER
#include "gtm_trigger_trc.h"
#endif

GBLDEF	sgm_info	*sgm_info_ptr;
GBLDEF	tp_region	*tp_reg_free_list;	/* Ptr to list of tp_regions that are unused */
GBLDEF  tp_region	*tp_reg_list;		/* Ptr to list of tp_regions for this transaction */

GBLREF	short			crash_count;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

void tp_set_sgm(void)
{
	sgm_info	*si;
	sgmnt_addrs	*csa;

	csa = cs_addrs;
	assert(csa == &FILE_INFO(gv_cur_region)->s_addrs);
	si = csa->sgm_info_ptr;
	assert(si->tp_csa == csa);
	assert(si->tp_csd == cs_data);
	if (!si->tp_set_sgm_done)
	{
		si->next_sgm_info = first_sgm_info;
		first_sgm_info = si;
		si->start_tn = csa->ti->curr_tn;
		if (csa->critical)
			si->crash_count = csa->critical->crashcnt;
		insert_region(gv_cur_region, &tp_reg_list, &tp_reg_free_list, SIZEOF(tp_region));
		/* In case triggers are supported, make sure we start with latest copy of file header's db_trigger_cycle
		 * to avoid unnecessary cdb_sc_triggermod type of restarts.
		 */
		GTMTRIG_ONLY(csa->db_trigger_cycle = csa->hdr->db_trigger_cycle);
		GTMTRIG_ONLY(DBGTRIGR((stderr, "tp_set_sgm: Updating csa->db_trigger_cycle to %d\n",
				       csa->db_trigger_cycle)));
		si->tp_set_sgm_done = TRUE;
		assert(0 == si->update_trans);
	}
	DBG_CHECK_IN_FIRST_SGM_INFO_LIST(si);
	sgm_info_ptr = si;
	crash_count = si->crash_count;
}
