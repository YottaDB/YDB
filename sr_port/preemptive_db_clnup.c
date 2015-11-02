/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"	/* for the RESET_GV_TARGET macro which in turn uses "memcmp" */

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "probe.h"
#include "gdsfhead.h"
#include "error.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "gdscc.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "interlock.h"
#include "preemptive_db_clnup.h"
#include "add_inter.h"
#include "gtmimagename.h"
#include "t_abort.h"
#include "dpgbldir.h"

GBLREF	gv_namehead		*reset_gv_target;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF  sgm_info                *first_sgm_info;
GBLREF	boolean_t		need_kip_incr;
GBLREF	uint4			update_trans;

/* container for all the common chores that need to be performed on error conditions */

#define CLEAR_CSE(GVNH)							\
{									\
	gv_namehead		*gvnh;					\
	srch_blk_status		*s;					\
									\
	gvnh = GVNH;							\
	if (NULL != gvnh)						\
	{								\
		for (s = &gvnh->hist.h[0]; s->blk_num; s++)		\
			s->cse = NULL;					\
	}								\
}

void preemptive_db_clnup(int preemptive_severe)
{
	sgmnt_addrs	*csa;
	sgm_info	*si;
	gd_region	*r_top, *reg;
	gd_addr		*addr_ptr;

	if (!dollar_tlevel && update_trans)
	{	/* It's possible we hit an error in the middle of an update, at which point we have
		 * a valid clue and non-NULL cse. However, this causes problems for subsequent
		 * transactions (see comment in t_begin). In particular we could end up pinning buffers
		 * unnecessarily. So clear the cse of any histories that may have been active during the update.
		 */
		CLEAR_CSE(gv_target);
		if ((NULL != gv_target) && (NULL != gv_target->gd_csa))
		{
			CLEAR_CSE(gv_target->gd_csa->dir_tree);
			GTMTRIG_ONLY(CLEAR_CSE(gv_target->gd_csa->hasht_tree));
		}
	}
	if (INVALID_GV_TARGET != reset_gv_target)
	{
		if (SUCCESS != preemptive_severe && INFO != preemptive_severe)
		{
			/* We know of a few cases in Unix where gv_target and gv_currkey could be out of sync at this point.
			 *   a) If we are inside trigger code which in turn does an update that does
			 *	reads of ^#t global and ends up in a restart. This restart would
			 *	in turn do a rts_error(TPRETRY) which would invoke mdb_condition_handler
			 *	that would in turn invoke preemptive_db_clnup which invokes this macro.
			 *	In this tp restart case though, it is ok for gv_target and gv_currkey
			 *	to be out of sync because they are going to be reset by tp_clean_up anyways.
			 *	So skip the dbg-only in-sync check.
			 *   b) If we are in gvtr_init reading the ^#t global and detect an error (e.g. TRIGINVCHSET)
			 *	gv_target after the reset would be pointing to a regular global whereas gv_currkey
			 *	would be pointing to ^#t. It is ok to be out-of-sync since in this case, we expect
			 *	mdb_condition_handler to be calling us. That has code to reset gv_currkey (and
			 *	cs_addrs/cs_data etc.) to reflect gv_target (i.e. get them back in sync).
			 * Therefore in Unix we pass SKIP_GVT_GVKEY_CHECK to skip the gvtarget/gvcurrkey out-of-sync check
			 * in RESET_GV_TARGET. In VMS we pass DO_GVT_GVKEY_CHECK as we dont yet know of an out-of-sync situation.
			 */
			RESET_GV_TARGET(UNIX_ONLY(SKIP_GVT_GVKEY_CHECK) VMS_ONLY(DO_GVT_GVKEY_CHECK));
		}
	}
	need_kip_incr = FALSE;	/* in case we got an error in t_end (e.g. GBLOFLOW), dont want this global variable to get
				 * carried over to the next non-TP transaction that this process does (e.g. inside an error trap).
				 */
	if (dollar_tlevel)
	{
		for (si = first_sgm_info;  si != NULL; si = si->next_sgm_info)
		{
			if (NULL != si->kip_csa)
			{
				csa = si->tp_csa;
				assert(si->tp_csa == si->kip_csa);
				CAREFUL_DECR_KIP(csa->hdr, csa, si->kip_csa);
			}
		}
	} else if (NULL != kip_csa && (NULL != kip_csa->hdr) && (NULL != kip_csa->nl))
		CAREFUL_DECR_KIP(kip_csa->hdr, kip_csa, kip_csa);
	if (IS_DSE_IMAGE)
	{	/* Release crit on any region that was obtained for the current erroring DSE operation.
		 * Take care NOT to release crits obtained by a previous CRIT -SEIZE command.
		 */
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
			{
				if (reg->open && !reg->was_open)
				{
					csa = &FILE_INFO(reg)->s_addrs;
					assert(csa->hold_onto_crit || !csa->dse_crit_seize_done);
					assert(!csa->hold_onto_crit || csa->now_crit);
					if (csa->now_crit && (!csa->hold_onto_crit || !csa->dse_crit_seize_done))
					{
						rel_crit(reg);
						csa->hold_onto_crit = FALSE;
						t_abort(reg, csa);	/* cancel mini-transaction if any in progress */
					}
				}
			}
		}
	}
}
