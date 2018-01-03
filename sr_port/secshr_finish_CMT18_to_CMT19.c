/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "add_inter.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"		/* needed for jnlpool_addrs_ptr_t */
#include "gtmsource.h"		/* needed for jnlpool_addrs_ptr_t */
#include "secshr_db_clnup.h"
#include "sec_shr_blk_build.h"
#include "cert_blk.h"		/* for CERT_BLK_IF_NEEDED macro */
#include "interlock.h"
#include "relqueopi.h"		/* for INSQTI and INSQHI macros */
#include "caller_id.h"
#include "db_snapshot.h"
#include "gdsbgtr.h"

GBLREF	cw_set_element		cw_set[];
GBLREF	unsigned char		cw_set_depth;
GBLREF	trans_num		start_tn;
GBLREF	unsigned int		cr_array_index;
#ifdef DEBUG
GBLREF	cache_rec_ptr_t		cr_array[]; /* Maximum number of blocks that can be in transaction */
#endif

GBLREF	uint4			process_id;

error_def(ERR_WCBLOCKED);

void secshr_finish_CMT18_to_CMT19(sgmnt_addrs *csa)
{
	boolean_t		is_bg;
	cache_rec_ptr_t		cr;
	char			*wcblocked_ptr;
	cw_set_element		*cs, *cs_ptr, *cs_top, *first_cw_set, *next_cs;
	node_local_ptr_t	cnl;
	sgm_info		*si;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_ptr;
	trans_num		currtn;
#	ifdef DEBUG
	boolean_t		cr_stopped_seen;
	cache_rec_ptr_t		*crArray;
	unsigned int		crArrayIndex;
#	endif

	if (FALSE == csa->t_commit_crit)	/* Step CMT19 is already done. Nothing more to do. */
		return;
	assert(!csa->now_crit || csa->hold_onto_crit);	/* Caller "secshr_db_clnup" should have ensured we released crit
							 * before coming here. The only exception is if csa->hold_onto_crit
							 * is set (e.g. dse) so account for that in the assert.
							 */
	csd = csa->hdr;
	is_bg = (dba_bg == csd->acc_meth);
	if (!is_bg)
	{	/* Step CMT18 already done as part of CMT10a for MM in "secshr_finish_CMT08_to_CMT14" */
		csa->t_commit_crit = FALSE;	/* Step CMT19 */
		return;	/* If MM, phase2 commit is already done */
	}
	/* Since we are in phase2 of commit (don't have crit), use value stored in "start_tn" or "si->start_tn" as currtn */
	if (dollar_tlevel)
	{
		si = csa->sgm_info_ptr;
		first_cw_set = si->first_cw_set;
		currtn = si->start_tn;	/* tn at which phase1 happened in crit */
	} else
	{
		first_cw_set = (0 != cw_set_depth) ? cw_set : NULL;
		currtn = start_tn;	/* tn at which phase1 happened in crit */
	}
	if (NULL != first_cw_set)
	{
		assert(T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit);
		assert(currtn < csa->ti->curr_tn);
		cnl = csa->nl;
		cs = first_cw_set;
		cs_top = (dollar_tlevel ? NULL : (cs + cw_set_depth));
		DEBUG_ONLY(cr_stopped_seen = FALSE;)
		for (next_cs = cs; cs_top != cs; cs = next_cs)
		{
			if (dollar_tlevel)
			{
				next_cs = next_cs->next_cw_set;
				TRAVERSE_TO_LATEST_CSE(cs);
			} else
				next_cs = cs + 1;
#			ifdef DEBUG
			if ((NULL != cs->cr) && (0 != cs->cr->stopped))
				cr_stopped_seen = TRUE;
#			endif
			if (gds_t_committed == cs->mode)
				continue;
			/* At this point, a positive value of "cs->old_mode" implies phase1 did not complete on "cs".
			 * This is possible for example if "secshr_finish_CMT08_to_CMT14" could not find a cr in the
			 * global buffers for this cs. In that case, skip this cr. Error messages corresponding to this
			 * missed block commit would have been recorded in "secshr_finish_CMT08_to_CMT14".
			 * There is one more possibility and that is if it there was a cse with mode > gds_t_committed
			 * (for example gds_t_write_root etc.) and phase1 was finished in t_end/tp_tend but an error occurred
			 * in phase2 when "secshr_db_clnup" took control. In that case, t_end/tp_tend would have set cs->old_mode
			 * to a positive value but that should be > gds_t_committed. Assert that.
			 */
			if (0 <= cs->old_mode)
			{
				assert(gds_t_committed < cs->old_mode);
				continue;
			}
			assert(gds_t_writemap != cs->mode);
			cr = cs->cr;
			ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)cr, csa);
			blk_ptr = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
			if (0 != secshr_finish_CMT18(csa, csd, is_bg, cs, blk_ptr, currtn))
				continue;	/* error during CMT18, move on to next cs */
		}
#		ifdef DEBUG
		if (!cr_stopped_seen)
		{	/* We did not pick any crs in "secshr_finish_CMT08_to_CMT14" (those with cr->stopped non-zero).
			 * This means we can be sure when phase2 of commit is done, all crs have been unpinned. Assert accordingly.
			 */
			if (dollar_tlevel)
			{
				crArray = si->cr_array;
				crArrayIndex = si->cr_array_index;
			} else
			{
				crArray = cr_array;
				crArrayIndex = cr_array_index;
			}
			ASSERT_CR_ARRAY_IS_UNPINNED(csd, crArray, crArrayIndex);
		}
#		endif
		if (dollar_tlevel)
			si->cr_array_index = 0;
		else
			cr_array_index = 0; /* Take this opportunity to reset cr_array_index */
		/* Now that phase2 of commit (all block builds) is done, set wc_blocked to trigger cache recovery */
		/* If csa->t_commit_crit is TRUE, even if csa->now_crit is FALSE, we might need cache
		 * cleanup (e.g. cleanup of orphaned cnl->wcs_phase2_commit_pidcnt counter in case
		 * a process gets shot in the midst of DECR_WCS_PHASE2_COMMIT_PIDCNT macro before
		 * decrementing the shared counter but after committing the transaction otherwise)
		 * so set wc_blocked.
		 */
		SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
		wcblocked_ptr = WCBLOCKED_PHASE2_CLNUP_LIT;
		BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_phase2_clnup);
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_STR(wcblocked_ptr),
			process_id, &csd->trans_hist.curr_tn, DB_LEN_STR(csa->region));
		/* Now that wc_blocked has been set, clean up the phase2 commit for this pid */
		if (csa->wcs_pidcnt_incremented)
			DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
		/* Phase 2 commits are completed for the current region. See if we had done a snapshot
		 * init (csa->snapshot_in_prog == TRUE). If so, try releasing the resources obtained
		 * while snapshot init.
		 */
		if (SNAPSHOTS_IN_PROG(csa))
		{
			assert(NULL != first_cw_set);
			SS_RELEASE_IF_NEEDED(csa, cnl);
		}
	}
	csa->t_commit_crit = FALSE;	/* Step CMT19 */
}
