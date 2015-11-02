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

#ifdef UNIX
#include "aswp.h"
#elif defined(VMS)
#include <descrip.h>
#endif

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
#include "secshr_db_clnup.h"
#include "t_commit_cleanup.h"
#include "process_deferred_stale.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs structure definition */
#include "send_msg.h"
#include "have_crit.h"

GBLREF	cache_rec_ptr_t		cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	unsigned int		cr_array_index;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	cw_set_element		cw_set[];
GBLREF	unsigned char		cw_set_depth;
GBLREF	uint4			dollar_trestart;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	sgm_info		*first_tp_si_by_ftok; /* List of participating regions in the TP transaction sorted on ftok order */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF	uint4			process_id;
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	uint4			update_trans;
#ifdef UNIX
GBLREF	jnl_gbls_t		jgbl;
#endif

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

#define	RESET_REG_SEQNO_IF_NEEDED(csa, jpl_csa)					\
{										\
	if (reg_seqno_reset)							\
	{									\
		assert(csa->hdr->reg_seqno <= (jnlpool_ctl->jnl_seqno + 1));	\
		assert(csa->now_crit && jpl_csa->now_crit);			\
		csa->hdr->reg_seqno = jnlpool_ctl->jnl_seqno;			\
	}									\
}

boolean_t t_commit_cleanup(enum cdb_sc status, int signal)
{
	boolean_t			update_underway, reg_seqno_reset = FALSE, release_crit;
	cache_rec_ptr_t			cr;
	sgm_info			*si;
	sgmnt_addrs			*csa, *jpl_csa = NULL;
	char				*trstr;
	gd_region			*xactn_err_region, *jpl_reg = NULL;
	cache_rec_ptr_t			*tp_cr_array;
	DEBUG_ONLY(unsigned int		lcl_t_tries;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(cdb_sc_normal != status);
	xactn_err_region = gv_cur_region;
	/* see comments in secshr_db_clnup for the commit logic flow as a sequence of steps in t_end and tp_tend and how
	 * t_commit_cleanup() and secshr_db_clnup() complement each other (one does the rollback and one the roll forward)
	 * update_underway is set to TRUE to indicate the commit is beyond rollback. It is set only if we hold crit on the region.
	 */
	update_underway = FALSE;
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
				if (T_UPDATE_UNDERWAY(csa))
				{
					update_underway = TRUE;
					break;
				}
			}
		}
	} else
	{
		trstr = "NON-TP";
		update_underway = (cs_addrs->now_crit && (UPDTRNS_TCOMMIT_STARTED_MASK & update_trans)
					|| T_UPDATE_UNDERWAY(cs_addrs));
		if (NULL != gv_target)	/* gv_target can be NULL in case of DSE MAPS command etc. */
			gv_target->clue.end = 0; /* in case t_end() had set history's tn to be "valid_thru++", undo it */
	}
	if (!update_underway)
	{	/* Rollback (undo) the transaction. the comments below refer to step numbers as documented in secshr_db_clnup */
		/* If we are here due to a restart (in t_end or tp_tend), we release crit as long as it is not the transition
		 * from 2nd to 3rd retry or 3rd to 3rd retry. However, if we are here because of a runtime error in t_end or tp_tend
		 * at a point where the transaction can be rolled backwards (update_underway = FALSE), we release crit before going
		 * to the error trap thereby avoiding any unintended crit hangs.
		 */
		release_crit = (0 == signal) ? NEED_TO_RELEASE_CRIT(t_tries, status) : TRUE;
		if ((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open)
		{
			csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
			if (csa->now_crit)
			{	/* reset any csa->hdr->early_tn like increments that might have occurred in jnlpool */
				assert((sm_uc_ptr_t)csa->critical == ((sm_uc_ptr_t)jnlpool_ctl + JNLPOOL_CTL_SIZE));
				if (jnlpool_ctl->early_write_addr != jnlpool_ctl->write_addr)
				{
					reg_seqno_reset = TRUE;	/* reset reg_seqnos of all regions to jnlpool_ctl->jnl_seqno */
					DEBUG_ONLY(jpl_csa = csa;)
					jnlpool_ctl->early_write_addr = jnlpool_ctl->write_addr; /* step (3) gets undone here */
				}
				assert(jnlpool_ctl->write == jnlpool_ctl->write_addr % jnlpool_ctl->jnlpool_size);
				if (!csa->hold_onto_crit)
					jpl_reg = jnlpool.jnlpool_dummy_reg;	/* note down to release crit later */
			}
		}
		if (dollar_tlevel)
		{	/* At this point we know a TP update is NOT underway. In this case, use "first_sgm_info" and not
			 * "first_tp_si_by_ftok" as the latter might be NULL even though we have gotten crit on all the
			 * regions and are in the final retry. In this case using "first_sgm_info" will guarantee that
			 * we release crit on all the regions.
			 */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
			{
				TP_CHANGE_REG(si->gv_cur_region);
				tp_cr_array = &si->cr_array[0];
				UNPIN_CR_ARRAY_ON_RETRY(tp_cr_array, si->cr_array_index);
				assert(!si->cr_array_index);
				csa = cs_addrs;
				if (si->update_trans)
				{
					RESET_EARLY_TN_IF_NEEDED(csa);		/* step (4) of the commit logic is undone here */
					RESET_REG_SEQNO_IF_NEEDED(csa, jpl_csa);/* step (5) of the commit logic is undone here */
				}
				assert(!csa->t_commit_crit);
				assert(!csa->now_crit || (csa->ti->curr_tn == csa->ti->early_tn));
				ASSERT_JNL_SEQNO_FILEHDR_JNLPOOL(csa->hdr, jnlpool_ctl); /* debug-only sanity check between
											  * seqno of filehdr and jnlpool */
				/* Do not release crit on the region until reg_seqno has been reset above. */
				assert(!csa->hold_onto_crit UNIX_ONLY(|| jgbl.onlnrlbk));
				if (!csa->hold_onto_crit && release_crit)
					rel_crit(gv_cur_region); /* step (1) of the commit logic is iteratively undone here */
			}
			/* If final retry and released crit (in the above loop), decrement t_tries to ensure that we dont have an
			 * out-of-design situation (with crit not being held in the final retry).
			 */
			if (release_crit && (CDB_STAGNATE <= t_tries))
				TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK; /* t_tries untouched for rollback and recover */
			UNIX_ONLY(assert(!jgbl.onlnrlbk || (lcl_t_tries == t_tries)));
			assert((lcl_t_tries == t_tries) || (t_tries == (CDB_STAGNATE - 1)));
			/* Do not release crit on jnlpool until reg_seqno has been reset above */
			RELEASE_JNLPOOL_LOCK_IF_NEEDED(jpl_reg);/* step (2) of the commit logic is undone here */
		} else
		{
			UNPIN_CR_ARRAY_ON_RETRY(cr_array, cr_array_index);
			assert(!cr_array_index);
			csa = cs_addrs;
			if (update_trans)
			{
				RESET_EARLY_TN_IF_NEEDED(csa);		/* step (4) of the commit logic is undone here */
				RESET_REG_SEQNO_IF_NEEDED(csa, jpl_csa);/* step (5) of the commit logic is undone here */
			}
			/* Do not release crit on jnlpool or the region until reg_seqno has been reset above */
			RELEASE_JNLPOOL_LOCK_IF_NEEDED(jpl_reg);/* step (2) of the commit logic is undone here */
			if (!csa->hold_onto_crit && release_crit)
				rel_crit(gv_cur_region);	/* step (1) of the commit logic is undone here */
		}
		DEBUG_ONLY(
			csa = (NULL == jpl_reg) ? NULL : &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
			assert((NULL == csa) || !csa->now_crit || csa->hold_onto_crit);
		)
		/* Do any pending buffer flush (wcs_wtstart) if we missed a flush timer. We should do this ONLY if we don't hold
		 * crit. Use release_crit for that purpose. The only case where release_crit is TRUE but we still hold crit is if
		 * the process wants to hold onto crit (for instance, DSE or ONLINE ROLLBACK). In that case, do the flush anyways.
		 */
		assert(!release_crit || (0 == have_crit(CRIT_HAVE_ANY_REG))
			UNIX_ONLY(|| jgbl.onlnrlbk) || (!dollar_tlevel && cs_addrs->hold_onto_crit));
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
		send_msg(VARLSTCNT(8) ERR_DBCOMMITCLNUP, 6, process_id, process_id, signal, trstr, DB_LEN_STR(xactn_err_region));
		/* if t_ch() (a condition handler) was driving this routine, then doing send_msg() here is not a good idea
		 * as it will overlay the current error message string driving t_ch(), but this case is an exception since
		 * we currently do not know of any way by which we will be in this "update_underway == TRUE" code if t_ch()
		 * calls us (there is an assert in t_ch to that effect in terms of testing the return value of this routine)
		 */
		secshr_db_clnup(COMMIT_INCOMPLETE);
		if (unhandled_stale_timer_pop)
			process_deferred_stale();
	}
	return update_underway;
}
