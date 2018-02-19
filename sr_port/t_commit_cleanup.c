/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "gdsbt.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cdb_sc.h"
#include "copy.h"
#include "error.h"
#include "gdscc.h"
#include "interlock.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs structure definition */
#include "secshr_db_clnup.h"
#include "t_commit_cleanup.h"
#include "process_deferred_stale.h"
#include "send_msg.h"
#include "have_crit.h"
#include "aswp.h"

GBLREF	cache_rec_ptr_t		cr_array[]; /* Maximum number of blocks that can be in transaction */
GBLREF	unsigned int		cr_array_index;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	cw_set_element		cw_set[];
GBLREF	unsigned char		cw_set_depth;
GBLREF	uint4			dollar_trestart;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	sgm_info		*first_tp_si_by_ftok; /* List of participating regions in the TP transaction sorted on ftok order */
GBLREF	tp_region		*tp_reg_list;	      /* List of tp_regions for this transaction */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	uint4			process_id;
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	uint4			update_trans;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		dse_running;

error_def(ERR_DBCOMMITCLNUP);

#define	RESET_EARLY_TN_IF_NEEDED(csa)						\
{										\
	assert(!csa->t_commit_crit);						\
	csa->t_commit_crit = FALSE;						\
	if (csa->now_crit && (csa->ti->curr_tn == csa->ti->early_tn - 1))	\
		csa->ti->early_tn = csa->ti->curr_tn;           		\
	assert(!csa->now_crit || csa->ti->curr_tn == csa->ti->early_tn);	\
}

#define	RELEASE_JNLPOOL_LOCK_IF_NEEDED(jpl_reg)				\
{									\
        sgmnt_addrs		*repl_csa;				\
									\
	if (NULL != jpl_reg)						\
	{								\
		repl_csa = &FILE_INFO(jpl_reg)->s_addrs;		\
		if (!repl_csa->hold_onto_crit)				\
			rel_lock(jpl_reg);				\
	}								\
}

#define	T_COMMIT_CLEANUP_DB(CR_ARRAY, CR_ARRAY_INDEX, CS_ADDRS, UPDATE_TRANS, JNLPOOL, JGBL, RELEASE_CRIT, GV_CUR_REGION)	\
MBSTART {															\
	cache_rec_ptr_t		*crArray;											\
	sgmnt_addrs		*csa;												\
	sgmnt_data_ptr_t	csd;												\
	jnl_buffer_ptr_t	jbp;												\
	int			index1, index2;											\
	jbuf_phase2_in_prog_t	*lastJbufCmt;											\
																\
	crArray = CR_ARRAY;													\
	UNPIN_CR_ARRAY_ON_RETRY(crArray, CR_ARRAY_INDEX);									\
	assert(!CR_ARRAY_INDEX);												\
	csa = CS_ADDRS;														\
	assert(!csa->t_commit_crit);												\
	assert(!csa->now_crit || (csa->ti->curr_tn == csa->ti->early_tn));							\
	ASSERT_JNL_SEQNO_FILEHDR_JNLPOOL(csa, JNLPOOL); /* debug-only sanity check between				\
								  * seqno of filehdr and jnlpool */				\
	csd = csa->hdr;														\
	/* Note: Below code is slightly similar to that in "mutex_salvage" */							\
	if (csa->now_crit && JNL_ENABLED(csd) && (csd->trans_hist.early_tn != csd->trans_hist.curr_tn))				\
	{	/* CMT04 finished but error happened before CMT08. Check if CMT06 needs to be undone */				\
		assert(csa->nl->update_underway_tn < csd->trans_hist.early_tn);							\
		assert(NULL != csa->jnl);											\
		assert(NULL != csa->jnl->jnl_buff);										\
		jbp = csa->jnl->jnl_buff;											\
		index1 = jbp->phase2_commit_index1;										\
		index2 = jbp->phase2_commit_index2;										\
		if (index1 != index2)												\
		{														\
			assert(jbp->freeaddr <= jbp->rsrv_freeaddr);								\
			DECR_PHASE2_COMMIT_INDEX(index2, JNL_PHASE2_COMMIT_ARRAY_SIZE);						\
			lastJbufCmt = &jbp->phase2_commit_array[index2];							\
			if (lastJbufCmt->process_id == process_id)								\
			{	/* CMT06 finished. So undo it as a whole */							\
				assert(lastJbufCmt->curr_tn == csd->trans_hist.curr_tn);					\
			/* 	NARSTODO : Invoke same cleanup code as in mutex.c to reset jb->freeaddr back */			\
				SET_JBP_RSRV_FREEADDR(jbp, lastJbufCmt->start_freeaddr);					\
				SHM_WRITE_MEMORY_BARRIER;/* see corresponding SHM_READ_MEMORY_BARRIER in "jnl_phase2_cleanup" */\
				jbp->phase2_commit_index2 = index2;	/* remove last commit entry */				\
				/* Undo Step (CMT06) complete */								\
			}													\
		}														\
	}															\
	if (UPDATE_TRANS)													\
		RESET_EARLY_TN_IF_NEEDED(csa);		/* Undo Step (CMT04) */							\
	assert(!csa->hold_onto_crit || JGBL.onlnrlbk || TREF(in_gvcst_redo_root_search) || dse_running);			\
	if (!csa->hold_onto_crit && RELEASE_CRIT)										\
		rel_crit(GV_CUR_REGION); 		/* Undo Step (CMT01) */							\
} MBEND

boolean_t t_commit_cleanup(enum cdb_sc status, int signal)
{
	boolean_t			update_underway, reg_seqno_reset = FALSE, release_crit;
	cache_rec_ptr_t			cr;
	sgm_info			*si, *jnlpool_si = NULL;
	sgmnt_addrs			*csa, *jpl_csa = NULL, *jnlpool_csa = NULL;
	jnlpool_addrs_ptr_t		save_jnlpool, save2_jnlpool, update_jnlpool = NULL;
	tp_region			*tr;
	char				*trstr;
	gd_region			*xactn_err_region, *jpl_reg = NULL;
	jnlpool_ctl_ptr_t		jpl;
	int				index1, index2;
	jpl_phase2_in_prog_t		*lastJplCmt;
	DEBUG_ONLY(unsigned int		lcl_t_tries;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(cdb_sc_normal != status);
	xactn_err_region = gv_cur_region;
	/* See comment at the top of "secshr_db_clnup" for the commit logic flow as a sequence of steps numbered CMTxx (where
	 * xx is 01, 02, etc.) in t_end/tp_tend and how "t_commit_cleanup" and "secshr_db_clnup" complement each other (one does
	 * the roll-back and one the roll-forward). update_underway is set to TRUE to indicate the commit is beyond rollback.
	 */
	update_underway = FALSE;
	save_jnlpool = jnlpool;
	if (cs_addrs->jnlpool && (jnlpool != cs_addrs->jnlpool))
	{
		jnlpool_csa = cs_addrs;
		jnlpool = cs_addrs->jnlpool;
	}
	if (dollar_tlevel)
	{
		trstr = "TP";
		/* Regions are committed in the ftok order using "first_tp_si_by_ftok". Also crit is released on each region
		 * as the commit completes. Take that into account while determining if update is underway. Note that this
		 * update_underway determining logic is shared by secshr_db_clnup as well so any changes here need to be made there.
		 */
		for (si = first_tp_si_by_ftok;  (NULL != si);  si = si->next_tp_si_by_ftok)
		{
			if (UPDTRNS_TCOMMIT_STARTED_MASK & si->update_trans)
			{	/* Two possibilities.
				 *	(a) case of duplicate set not creating any cw-sets but updating db curr_tn++.
				 *	(b) Have completed commit for this region and have released crit on this region.
				 *		(in a potentially multi-region TP transaction).
				 * In either case, update is underway and the transaction cannot be rolled back.
				 */
				update_underway = TRUE;
				break;
			}
			if (NULL != si->first_cw_set)
			{
				csa = si->tp_csa;
				if (csa->jnlpool && (jnlpool != csa->jnlpool))
				{
					assert(!update_jnlpool || (!csa->jnlpool || (update_jnlpool == csa->jnlpool)));
					jnlpool_si = si;
					jnlpool_csa = csa;
					jnlpool = csa->jnlpool;
				}
				if (T_UPDATE_UNDERWAY(csa))
				{
					update_underway = TRUE;
					break;
				}
				if (!update_jnlpool && REPL_ALLOWED(csa))
					update_jnlpool = jnlpool;
			}
		}
	} else
	{
		trstr = "NON-TP";
		assert(!(cs_addrs->now_crit && (UPDTRNS_TCOMMIT_STARTED_MASK & update_trans)) || T_UPDATE_UNDERWAY(cs_addrs));
		update_underway = T_UPDATE_UNDERWAY(cs_addrs);
		if (NULL != gv_target)			/* gv_target can be NULL in case of DSE MAPS command etc. */
			gv_target->clue.end = 0;	/* in case t_end() had set history's tn to be "valid_thru++", undo it */
		update_jnlpool = cs_addrs->jnlpool ? cs_addrs->jnlpool : jnlpool;
	}
	if (!update_underway)
	{	/* Rollback (undo) the transaction. the comments below refer to CMTxx step numbers described in secshr_db_clnup.
		 * At this point we know an update is not underway. That means we got an error BEFORE Step CMT08.
		 * If we are here due to a restart (in t_end or tp_tend), we release crit as long as it is not the transition
		 * from 2nd to 3rd retry or 3rd to 3rd retry. However, if we are here because of a runtime error in t_end or tp_tend
		 * at a point where the transaction can be rolled backwards (update_underway = FALSE), we release crit before going
		 * to the error trap thereby avoiding any unintended crit hangs.
		 */
		release_crit = (0 == signal) ? NEED_TO_RELEASE_CRIT(t_tries, status) : TRUE;
		assert(!dollar_tlevel || !update_jnlpool || !jnlpool_si || (!jnlpool || (update_jnlpool == jnlpool)));
		if (update_jnlpool && update_jnlpool->jnlpool_dummy_reg && update_jnlpool->jnlpool_dummy_reg->open)
		{
			csa = &FILE_INFO(update_jnlpool->jnlpool_dummy_reg)->s_addrs;
			if (csa->now_crit)
			{	/* Undo Step CMT03. Note: The below code is similar to that in "mutex_salvage" for the jnlpool */
				assert(update_jnlpool->jnlpool_ctl);
				jpl = update_jnlpool->jnlpool_ctl;
				index1 = jpl->phase2_commit_index1;
				index2 = jpl->phase2_commit_index2;
				if (index1 != index2)
				{
					assert(jpl->write_addr <= jpl->rsrv_write_addr);
					DECR_PHASE2_COMMIT_INDEX(index2, JPL_PHASE2_COMMIT_ARRAY_SIZE);
					lastJplCmt = &jpl->phase2_commit_array[index2];
					if (lastJplCmt->process_id == process_id)
					{	/* An error occurred after CMT03 but before CMT07. Undo CMT03 */
						assert(lastJplCmt->jnl_seqno == jpl->jnl_seqno);
						jpl->rsrv_write_addr = lastJplCmt->start_write_addr;
						jpl->lastwrite_len = lastJplCmt->prev_jrec_len;
						SHM_WRITE_MEMORY_BARRIER; /* similar layout as UPDATE_JPL_RSRV_WRITE_ADDR */
						jpl->phase2_commit_index2 = index2;	/* remove last commit entry */
					}
				}
				if (!csa->hold_onto_crit)
					jpl_reg = update_jnlpool->jnlpool_dummy_reg;	/* note down to release crit later */
			}
		}
		if (dollar_tlevel)
		{	/* At this point we know a TP update is NOT underway. In this case, use "first_sgm_info" and not
			 * "first_tp_si_by_ftok" as the latter might be NULL even though we have gotten crit on all the
			 * regions and are in the final retry. In this case using "first_sgm_info" will guarantee that
			 * we release crit on all the regions.
			 */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			save2_jnlpool = jnlpool;
			for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
			{
				TP_CHANGE_REG(si->gv_cur_region);		/* sets jnlpool */
				/* Undo CMT06, CMT04 and CMT01 */
				T_COMMIT_CLEANUP_DB(&si->cr_array[0], si->cr_array_index, cs_addrs, si->update_trans,	\
									jnlpool, jgbl, release_crit, gv_cur_region);
			}
			jnlpool = save2_jnlpool;
			if (release_crit)
			{	/* If final retry and released crit (in the above loop), do the following
				 * Decrement t_tries to ensure that we don't have an out-of-design situation
				 * (with crit not being held in the final retry).
				 */
				if (CDB_STAGNATE <= t_tries)
					TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK; /* t_tries untouched for rollback and recover */
				/* Above, we released crit on all regions in "first_sgm_info" (list of regions referenced in the
				 * current try/retry of this TP transaction). But it is possible "tp_reg_list" (list of regions
				 * referenced across all tries/retries of this TP transaction until now) contains a few more
				 * regions on which we have crit . In that case we need to release crit on those regions as well.
				 * Types of activity that could lead to this situation:
				 *  - M locks
				 *  - Regions from which globals are read but not updated
				 */
				for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
				{
					assert(tr->reg->open);
					csa = (sgmnt_addrs *)&FILE_INFO(tr->reg)->s_addrs;
					assert(!csa->hold_onto_crit || jgbl.onlnrlbk);
					if (!csa->hold_onto_crit && csa->now_crit)
						rel_crit(tr->reg);	/* Undo Step (CMT01) */
				}
			}
			assert(!jgbl.onlnrlbk || (lcl_t_tries == t_tries));
			assert((lcl_t_tries == t_tries) || (t_tries == (CDB_STAGNATE - 1)));
			RELEASE_JNLPOOL_LOCK_IF_NEEDED(jpl_reg);	/* Undo Step (CMT02) */
		} else
		{
			/* Undo CMT06, CMT04 and CMT01 */
			T_COMMIT_CLEANUP_DB(cr_array, cr_array_index, cs_addrs, update_trans,			\
							jnlpool, jgbl, release_crit, gv_cur_region);
			RELEASE_JNLPOOL_LOCK_IF_NEEDED(jpl_reg);	/* Undo Step (CMT02) */
		}
#		ifdef DEBUG
		csa = (NULL == jpl_reg) ? NULL : &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs;
		assert((NULL == csa) || !csa->now_crit || csa->hold_onto_crit);
#		endif
		/* Do any pending buffer flush (wcs_wtstart) if we missed a flush timer. We should do this ONLY if we don't hold
		 * crit. Use release_crit for that purpose. The only case where release_crit is TRUE but we still hold crit is if
		 * the process wants to hold onto crit (for instance, DSE or ONLINE ROLLBACK). In that case, do the flush anyways.
		 */
		assert(!release_crit || (0 == have_crit(CRIT_HAVE_ANY_REG))
			|| jgbl.onlnrlbk || (!dollar_tlevel && cs_addrs->hold_onto_crit));
		if (release_crit && unhandled_stale_timer_pop)
			process_deferred_stale();
	} else
	{	/* Roll forward (complete the partial commit of) the transaction by invoking secshr_db_clnup(). At this point, we
		 * don't know of any reason why signal should be 0 as that would indicate that we encountered a runtime error in
		 * t_end/tp_tend and yet decided to roll forward the transaction. So, assert accordingly and if ever this happens,
		 * we need to revisit the commit logic and fix the error as we are past the point where we no longer can roll-back
		 * the transaction.
		 */
		assert(0 == signal);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DBCOMMITCLNUP, 6, process_id, process_id, signal, trstr,
				DB_LEN_STR(xactn_err_region));
		/* if t_ch() (a condition handler) was driving this routine, then doing send_msg() here is not a good idea
		 * as it will overlay the current error message string driving t_ch(), but this case is an exception since
		 * we currently do not know of any way by which we will be in this "update_underway == TRUE" code if t_ch()
		 * calls us (there is an assert in t_ch to that effect in terms of testing the return value of this routine)
		 */
		secshr_db_clnup(COMMIT_INCOMPLETE);
		if (unhandled_stale_timer_pop)
			process_deferred_stale();
	}
	if (jnlpool != save_jnlpool)
		jnlpool = save_jnlpool;
	return update_underway;
}
