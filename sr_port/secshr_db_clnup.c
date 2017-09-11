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

#include "gtm_inet.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"
#include "gdsblkops.h"
#include "gdsbml.h"
#include "gdskill.h"
#include "copy.h"
#include "interlock.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "io.h"
#include "gtmsecshr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "is_proc_alive.h"
#include "aswp.h"
#include "util.h"
#include "compswap.h"
#include "mutex.h"
#include "repl_instance.h"	/* needed for JNLDATA_BASE_OFF macro */
#include "mupipbckup.h"		/* needed for backup_block prototype */
#include "cert_blk.h"		/* for CERT_BLK_IF_NEEDED macro */
#include "relqueopi.h"		/* for INSQTI and INSQHI macros */
#include "caller_id.h"
#include "sec_shr_blk_build.h"
#include "sec_shr_map_build.h"
#include "add_inter.h"
#include "send_msg.h"	/* for send_msg prototype */
#include "secshr_db_clnup.h"
#include "gdsbgtr.h"
#include "memcoherency.h"
#include "shmpool.h"
#include "wbox_test_init.h"
#include "db_snapshot.h"
#include "muextr.h"
#include "mupip_reorg.h"
#include "op.h"
#include "dpgbldir.h"		/* for "get_next_gdr" */
#include "t_abort.h"

#define FLUSH 1

#define	WCBLOCKED_NOW_CRIT_LIT		"wcb_secshr_db_clnup_now_crit"
#define	WCBLOCKED_WBUF_DQD_LIT		"wcb_secshr_db_clnup_wbuf_dqd"
#define	WCBLOCKED_PHASE2_CLNUP_LIT	"wcb_secshr_db_clnup_phase2_clnup"

#ifdef DEBUG_CHECK_LATCH
#  define DEBUG_LATCH(x) x
#else
#  define DEBUG_LATCH(x)
#endif

#define	RELEASE_LATCH_IF_OWNER_AND_EXITING(X, is_exiting)			\
MBSTART {									\
	if (is_exiting && ((X)->u.parts.latch_pid == process_id))		\
	{									\
		SET_LATCH_GLOBAL(X, LOCK_AVAILABLE);				\
		DEBUG_LATCH(util_out_print("Latch cleaned up", FLUSH));		\
	}									\
} MBEND

GBLREF	boolean_t		certify_all_blocks;
GBLREF	boolean_t		need_kip_incr;
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_region		*gv_cur_region;		/* for the LOCK_HIST macro in the RELEASE_BUFF_UPDATE_LOCK macro */
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	node_local_ptr_t	locknl;			/* set explicitly before invoking RELEASE_BUFF_UPDATE_LOCK macro */
GBLREF	sgm_info		*first_sgm_info;	/* List of all regions (unsorted) in TP transaction */
GBLREF	sgm_info		*first_tp_si_by_ftok;	/* List of READ or UPDATED regions sorted on ftok order */
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs 		*kip_csa;
GBLREF	short			crash_count;
GBLREF	trans_num		start_tn;
GBLREF	uint4			process_id;
GBLREF	uint4			update_trans;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		cr_array_index;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	volatile int4		crit_count;
GBLREF	int4			strm_index;
GBLREF	jnl_fence_control	jnl_fence_ctl;

#ifdef DEBUG
GBLREF	volatile boolean_t	in_wcs_recover; /* TRUE if in "wcs_recover" */
GBLREF	boolean_t		dse_running;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	cache_rec_ptr_t		cr_array[]; /* Maximum number of blocks that can be in transaction */
#endif

error_def(ERR_WCBLOCKED);

typedef enum
{
	REG_COMMIT_UNSTARTED = 0,/* indicates that GT.M has not committed even one cse in this region */
	REG_COMMIT_PARTIAL,	 /* indicates that GT.M has committed at least one but not all cses for this region */
	REG_COMMIT_COMPLETE	 /* indicates that GT.M has already committed all cw-set-elements for this region */
} commit_type;

/* secshr_db_clnup can be called with one of the following three values for "secshr_state"
 *
 * 	a) NORMAL_TERMINATION   --> We are called from the exit-handler for precautionary cleanup.
 * 				    We should NEVER be in the midst of a database update in this case.
 * 	b) COMMIT_INCOMPLETE    --> We are called from t_commit_cleanup.
 * 				    We should ALWAYS be in the midst of a database update in this case.
 * 	c) ABNORMAL_TERMINATION --> This is currently VMS ONLY. This process received a STOP/ID.
 * 				    We can POSSIBLY be in the midst of a database update in this case.
 * 				    When UNIX boxes allow kernel extensions, it can then handle "kill -9".
 *
 * If we are in the midst of a database update, then depending on where we are in the commit logic
 * 	we need to ROLL-BACK (undo the partial commit) or ROLL-FORWARD (complete the partial commit) the database update.
 * t_commit_cleanup handles the ROLL-BACK and secshr_db_clnup handles the ROLL-FORWARD
 *
 * For all error conditions in the database commit logic, t_commit_cleanup gets control first.
 * If then determines whether to do a ROLL-BACK or a ROLL-FORWARD.
 * If a ROLL-BACK needs to be done, then t_commit_cleanup handles it all by itself and we will not come here.
 * If a ROLL-FORWARD needs to be done, then t_commit_cleanup invokes secshr_db_clnup.
 * 	In this case, secshr_db_clnup will be called with a "secshr_state" value of "COMMIT_INCOMPLETE".
 *
 * Irrespective of whether we are in the midst of a database commit or not, t_commit_cleanup does not get control.
 * Since the process can POSSIBLY be in the midst of a database update while it was STOP/IDed,
 * 	the logic for determining whether it is a ROLL-BACK or a ROLL-FORWARD needs to also be in secshr_db_clnup.
 * If it is determined that a ROLL-FORWARD needs to be done, secshr_db_clnup takes care of it by itself.
 * But if a ROLL-BACK needs to be done, then secshr_db_clnup DOES NOT invoke t_commit_cleanup.
 * Instead it sets cnl->wc_blocked to TRUE thereby ensuring the next process that gets CRIT does a cache recovery
 * 	which will take care of doing more than the ROLL-BACK that t_commit_cleanup would have otherwise done.
 *
 * In order to understand how a ROLL-BACK or ROLL-FORWARD decision is made, the commit flow needs to be understood first.
 * The commit logic flow in t_end & tp_tend can be captured as follows.
 * Note that
 *	a) In t_end there is only one region in the "For each region" loop. And "si->update_trans" == "update_trans".
 *	b) All jnlpool steps below apply only if replication is turned ON.
 *	c) For MM, the jnl and db commits happen inside crit i.e. no phase2 outside crit like BG.
 *
 * (CMT01) Get crit on all regions (UPDATED & NON-UPDATED)
 * (CMT02) Get crit on jnlpool
 * (CMT03) Reserve space in JNLPOOL for journal records (UPDATE_JPL_RSRV_WRITE_ADDR macro)
 *         For each UPDATED region
 *         {
 * (CMT04)     csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
 * (CMT05)     csa->t_commit_crit = T_COMMIT_CRIT_PHASE0;
 *         }
 *         For each UPDATED & JOURNALED region
 *         {
 * (CMT06)     Reserve space in JNLBUFF for journal records PHASE1 (UPDATE_JRS_RSRV_FREEADDR macro)
 *         }
 * (CMT06a) If (MM or (BG && jnlbuff-overflow)) Do Step CMT16 here instead of later
 * (CMT06b) If (MM) Do Step CMT17 here instead of later
 * (CMT07) jnlpool_ctl->jnl_seqno++; jnlpool_ctl->strm_seqno[]++ if supplementary; (SET_JNL_SEQNO macro)
 *         For each UPDATED region
 *         {
 * (CMT08)     csa->t_commit_crit = T_COMMIT_CRIT_PHASE1; cnl->update_underway_tn = csd->trans_hist.curr_tn;
 * (CMT09)     If replication is ON, csd->reg_seqno = jnlpool_ctl->jnl_seqno + 1; csd->strm_reg_seqno[] = xxx
 * (CMT10)     Commit all cw-set-elements of this region PHASE1 (inside crit)	// bg_update_phase1 or mm_update
 * (CMT10a)    If (BG && (bitmap cw-set-element of this region)) commit it PHASE2 (inside crit)	// bg_update_phase2
 * (CMT11)     si->update_trans |= UPDTRNS_TCOMMIT_STARTED_MASK;
 * (CMT12)     csd->trans_hist.curr_tn++;
 * (CMT13)     csa->t_commit_crit = T_COMMIT_CRIT_PHASE2;
 * (CMT14)     Release crit on region
 *         }
 * (CMT15) Release crit on jnlpool
 *         For each UPDATED & JOURNALED region
 * (CMT16)     If (BG) Write journal records in JNLBUFF & JNLPOOL. PHASE2 (outside crit). Mark write complete in JNLBUFF.
 * (CMT17) If (BG) Mark journal record write complete in JNLPOOL.
 *         For each participating region being UPDATED
 *         {
 * (CMT18)     Commit all cw-set-elements of this region PHASE2 (outside crit)	// bg_update_phase2
 * (CMT19)     csa->t_commit_crit = FALSE;
 *         }
 *
 * If a transaction has proceeded to Step (CMT08) for at least one region, then "tp_update_underway" is set to TRUE
 * and the transaction cannot be rolled back but has to be committed. Otherwise the transaction is rolled back.
 * This is for the case where the process encounters an error in the midst of commit. In this case, we are guaranteed
 * a clean recovery by secshr_db_clnup() & wcs_recover(). Database integrity is guaranteed.
 * But if the process in the midst of commit is abnormally killed (e.g. kill -9), then "mutex_salvage",
 * "jnl_phase2_salvage" and "repl_phase2_salvage" try to cleanup various pieces of the leftover mess but
 * we are not guaranteed db integrity (i.e. CMT18 cannot be currently easily redone by a different process).
 */

void secshr_db_clnup(enum secshr_db_state secshr_state)
{
	unsigned char		*chain_ptr;
	char			*wcblocked_ptr;
	boolean_t		is_bg, do_accounting, first_time = TRUE, is_exiting;
	boolean_t		tp_update_underway = FALSE;	/* set to TRUE if TP commit was in progress or complete */
	boolean_t		non_tp_update_underway = FALSE;	/* set to TRUE if non-TP commit was in progress or complete */
	boolean_t		update_underway = FALSE;	/* set to TRUE if either TP or non-TP commit was underway */
	boolean_t		set_wc_blocked = FALSE;		/* set to TRUE if cnl->wc_blocked needs to be set */
	boolean_t		donot_reset_data_invalid;	/* set to TRUE in case cr->data_invalid was TRUE in phase2 */
	int			max_bts, old_mode;
	unsigned int		lcnt;
	cache_rec_ptr_t		clru, cr, cr_alt, cr_top, start_cr, actual_cr;
	cache_que_heads_ptr_t	cache_state;
	cw_set_element		*cs, *cs_ptr, *cs_top, *first_cw_set, *nxt, *orig_cs;
	gd_addr			*gd_hdr;
	gd_region		*reg, *reg_top, *save_gv_cur_region;
	jnl_buffer_ptr_t	jbp;
	off_chain		chain;
	sgm_info		*si, *save_si;
	sgmnt_addrs		*csa, *tmp_csa, *repl_csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		blk_ptr;
	blk_hdr_ptr_t		blk_hdr_ptr;
	jnlpool_ctl_ptr_t	jpl;
	pid_t			pid;
	sm_uc_ptr_t		bufstart;
	int4			bufindx;	/* should be the same type as "csd->bt_buckets" */
	commit_type		this_reg_commit_type;	/* indicate the type of commit of a given region in a TP transaction */
	gv_namehead		*gvtarget;
	srch_blk_status		*t1;
	trans_num		currtn;
	int4			n;
	snapshot_context_ptr_t	lcl_ss_ctx;
	cache_rec_ptr_t		snapshot_cr;
	uint4			blk_size;
	jbuf_rsrv_struct_t	*jrs;
	seq_num			strm_seqno;
#	ifdef DEBUG
	cache_rec_ptr_t		*crArray;
	unsigned int		crArrayIndex;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_gv_cur_region = gv_cur_region;	/* save it for use at function end in case code in between changes this variable */
	is_exiting = (NORMAL_TERMINATION == secshr_state);
	currtn = start_tn;
	if (dollar_tlevel)
	{	/* Determine update_underway for TP transaction. A similar check is done in t_commit_cleanup as well.
		 * Regions are committed in the ftok order using "first_tp_si_by_ftok". Also crit is released on each region
		 * as the commit completes. Take that into account while determining if update is underway.
		 */
		for (si = first_tp_si_by_ftok; NULL != si; si = si->next_tp_si_by_ftok)
		{
			if (UPDTRNS_TCOMMIT_STARTED_MASK & si->update_trans)
			{	/* Two possibilities.
				 *	(a) case of duplicate set not creating any cw-sets but updating db curr_tn++.
				 *	(b) Have completed commit for this region and have released crit on this region.
				 *		(in a potentially multi-region TP transaction).
				 * In either case, update is underway and the transaction cannot be rolled back.
				 */
				tp_update_underway = TRUE;
				update_underway = TRUE;
				break;
			}
			if (NULL != si->first_cw_set)
			{
				csa = si->tp_csa;
				assert(NULL != csa);
				if (T_UPDATE_UNDERWAY(csa))
				{
					tp_update_underway = TRUE;
					update_underway = TRUE;
					break;
				}
			}
		}
	} else
	{	/* Determine update_underway for non-TP transaction */
		if (NULL != cs_addrs)
		{
			assert(!(cs_addrs->now_crit && (UPDTRNS_TCOMMIT_STARTED_MASK & update_trans))
									|| T_UPDATE_UNDERWAY(cs_addrs));
			if (T_UPDATE_UNDERWAY(cs_addrs))
			{
				non_tp_update_underway = TRUE;	/* non-tp update was underway */
				update_underway = TRUE;
			}
		}
	}
	/* Assert that if we had been called from t_commit_cleanup, we independently concluded that update is underway
	 * (as otherwise t_commit_cleanup would not have called us)
	 */
	assert((COMMIT_INCOMPLETE != secshr_state) || update_underway);
	for (gd_hdr = get_next_gdr(NULL); NULL != gd_hdr; gd_hdr = get_next_gdr(gd_hdr))
	{
		for (reg = gd_hdr->regions, reg_top = reg + gd_hdr->n_regions;  reg < reg_top;  reg++)
		{
			if (!reg->open || reg->was_open)
				continue;
			if (!IS_REG_BG_OR_MM(reg))
				continue;
			csa = REG2CSA(reg);
			if (NULL == csa)
				continue;
			assert((reg->read_only && !csa->read_write) || (!reg->read_only && csa->read_write));
			csd = csa->hdr;
			assert(NULL != csd);
			cnl = csa->nl;
			assert(NULL != cnl);
			is_bg = (csd->acc_meth == dba_bg);
			do_accounting = FALSE;	/* used by SECSHR_ACCOUNTING macro */
			/* Do SECSHR_ACCOUNTING only if holding crit. This is so we avoid another process' normal termination call
			 * to "secshr_db_clnup" from overwriting whatever important information we wrote. If we are in crit,
			 * for the next process to overwrite us it needs to get crit which in turn will invoke wcs_recover
			 * which in turn will send whatever we wrote (using SECSHR_ACCOUNTING) to the syslog).
			 * Also cannot update csd if MM and read-only. take care of that too.
			 */
			if (csa->now_crit && (csa->read_write || is_bg))
			{	/* start accounting */
				cnl->secshr_ops_index = 0;
				do_accounting = TRUE;	/* used by SECSHR_ACCOUNTING macro */
			}
			SECSHR_ACCOUNTING(do_accounting, 5);	/* 5 is the number of arguments following including self */
			SECSHR_ACCOUNTING(do_accounting, __LINE__);
			SECSHR_ACCOUNTING(do_accounting, process_id);
			SECSHR_ACCOUNTING(do_accounting, secshr_state);
			SECSHR_ACCOUNTING(do_accounting, csd->trans_hist.curr_tn);
			csa->ti = &csd->trans_hist;	/* correct it in case broken */
			if (is_exiting)
			{	/* If we hold any latches in the node_local area, release them. Note we do not check
				 * db_latch here because it is never used by the compare and swap logic but rather
				 * the aswp logic. Since it is only used for the 3 state cache record lock and
				 * separate recovery exists for it, we do not do anything with it here.
				 */
				RELEASE_LATCH_IF_OWNER_AND_EXITING(&cnl->wc_var_lock, is_exiting);
				assert(ABNORMAL_TERMINATION != secshr_state);
				/* Note: In case of "kill -9", cnl->wcs_timers & cnl->ref_cnt will stay uncleaned */
				if ((csa->in_wtstart) && (0 < cnl->in_wtstart))
				{
					DECR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);
					assert(0 < cnl->intent_wtstart);
					if (0 < cnl->intent_wtstart)
						DECR_CNT(&cnl->intent_wtstart, &cnl->wc_var_lock);
				}
				csa->in_wtstart = FALSE;	/* Let wcs_wtstart run for exit processing */
				if (cnl->wcsflu_pid == process_id)
					cnl->wcsflu_pid = 0;
			}
			set_wc_blocked = FALSE;
			if (is_bg)
			{
				assert(cnl->sec_size);
				cache_state = csa->acc_meth.bg.cache_state;
				RELEASE_LATCH_IF_OWNER_AND_EXITING(&cache_state->cacheq_active.latch, is_exiting);
				start_cr = cache_state->cache_array + csd->bt_buckets;
				bufstart = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, start_cr->buffaddr);
				max_bts = csd->n_bts;
				cr_top = start_cr + max_bts;
				if (is_exiting)
				{
#					ifdef DEBUG
					if (gtm_white_box_test_case_enabled && (reg == gd_hdr->regions)
						&& (WBTEST_SIGTSTP_IN_T_QREAD == gtm_white_box_test_case_number))
					{
						assert((NULL != TREF(block_now_locked))
							&& ((TREF(block_now_locked))->r_epid == process_id));
					}
#					endif
					if (NULL != (cr = TREF(block_now_locked)))	/* done by region to ensure access */
					{	/* The following is potentially thread-specific rather than process-specific */
						TREF(block_now_locked) = NULL;
						RELEASE_LATCH_IF_OWNER_AND_EXITING(&cr->rip_latch, is_exiting);
						if ((cr->r_epid == process_id) && (0 == cr->dirty) && (0 == cr->in_cw_set))
						{	/* increment cycle for blk number changes (for tp_hist) */
							cr->cycle++;
							cr->blk = CR_BLKEMPTY;
							/* ensure no bt points to this cr for empty blk */
							assert(0 == cr->bt_index);
							/* Don't mess with ownership the I/O may not yet be cancelled.
							 * Ownership will be cleared by whoever gets stuck waiting for the buffer.
							 */
						}
					}
				}
			}
			first_cw_set = cs = NULL;
			/* If tp_update_underway has been determined to be TRUE, then we are guaranteed we have a well formed
			 * ftok ordered linked list ("first_tp_si_by_ftok") so we can safely use this.
			 */
			if (tp_update_underway)
			{	/* This is constructed to deal with the issue of reg != si->gv_cur_region due to the possibility
				 * of multiple global directories pointing to regions that resolve to the same physical file;
				 * was_open prevents processing the segment more than once, so this code matches on the file
				 * rather than the region to make sure that it gets processed at least once.
				 */
				for (si = first_tp_si_by_ftok; NULL != si; si = si->next_tp_si_by_ftok)
				{
					if (FILE_CNTL(si->gv_cur_region) == FILE_CNTL(reg))
					{
						cs = si->first_cw_set;
						TRAVERSE_TO_LATEST_CSE(cs);
						first_cw_set = cs;
						break;
					}
				}
			} else if (!dollar_tlevel && T_UPDATE_UNDERWAY(csa))
			{	/* We have reached Step (CMT08). ROLL-FORWARD the commit unconditionally */
				if (0 != cw_set_depth)
				{
					first_cw_set = cs = cw_set;
					cs_top = cs + cw_set_depth;
				}
				/* else is the case where we had a duplicate set that did not update any cw-set */
				assert(!tp_update_underway);
				assert(non_tp_update_underway);	/* should have already determined update is underway */
				/* This is a situation where we are in non-TP and have a region that we hold
				 * crit in and are in the midst of commit but this region was not the current
				 * region when we entered secshr_db_clnup. This is an out-of-design situation
				 * that we want to catch.
				 */
				assertpro(non_tp_update_underway);
				non_tp_update_underway = TRUE;	/* just in case */
				update_underway = TRUE;		/* just in case */
			}
			assert(!tp_update_underway || (NULL == first_cw_set) || (NULL != si));
			/* It is possible that we were in the midst of a non-TP commit for this region at or past Step (CMT13)
			 * but first_cw_set is NULL. This is a case of duplicate SET with zero cw_set_depth. In this case,
			 * don't have any cw-set-elements to commit. The only thing remaining to do is Step (CMT14) through
			 * Step (CMT17) which is done later in this function.
			 */
			assert((FALSE == csa->t_commit_crit) || (T_COMMIT_CRIT_PHASE0 == csa->t_commit_crit)
				|| (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit) || (T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit));
			assert((NULL == first_cw_set) || csa->now_crit || csa->t_commit_crit || tp_update_underway);
			do     /* have a dummy loop to be able to use "break" for various codepaths below */
			{
				if (!csa->now_crit && !csa->t_commit_crit)
					break;	/* Skip processing region in case of a multi-region TP transaction
						 * where this region is already committed.
						 */
				SECSHR_ACCOUNTING(do_accounting, 6);
				SECSHR_ACCOUNTING(do_accounting, __LINE__);
				SECSHR_ACCOUNTING(do_accounting, csa->now_crit);
				SECSHR_ACCOUNTING(do_accounting, csa->t_commit_crit);
				SECSHR_ACCOUNTING(do_accounting, csd->trans_hist.early_tn);
				SECSHR_ACCOUNTING(do_accounting, csd->trans_hist.curr_tn);
				assert(non_tp_update_underway || tp_update_underway || ((NULL == first_cw_set) && is_exiting));
				assert(!non_tp_update_underway || !tp_update_underway);
				assert((T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit) || csa->now_crit);
				if (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit)
				{	/* in PHASE1 so hold crit AND have noted down valid value in csa->prev_free_blks */
					/* for normal termination we should not have been in the midst of commit */
					assert(!is_exiting || WBTEST_ENABLED(WBTEST_SLEEP_IN_WCS_WTSTART));
					assert(csa->now_crit);
					csd->trans_hist.free_blocks = csa->prev_free_blks;
				}
				SECSHR_ACCOUNTING(do_accounting, tp_update_underway ? 6 : 7);
				SECSHR_ACCOUNTING(do_accounting, __LINE__);
				SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)first_cw_set);
				SECSHR_ACCOUNTING(do_accounting, tp_update_underway);
				SECSHR_ACCOUNTING(do_accounting, non_tp_update_underway);
				if (!tp_update_underway)
				{
					SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs_top);
					SECSHR_ACCOUNTING(do_accounting, cw_set_depth);
					jrs = TREF(nontp_jbuf_rsrv);
				} else
				{
					SECSHR_ACCOUNTING(do_accounting, si->cw_set_depth);
					this_reg_commit_type = REG_COMMIT_UNSTARTED; /* assume GT.M did no commits in this region */
					jrs = si->jbuf_rsrv_ptr;
					/* Note that "this_reg_commit_type" is uninitialized if "tp_update_underway" is not TRUE
					 * so should always be used within an "if (tp_update_underway)".
					 */
				}
				if (NEED_TO_FINISH_JNL_PHASE2(jrs))
					FINISH_JNL_PHASE2_IN_JNLBUFF(csa, jrs);	/* Roll forward CMT16 */
				if (NULL == first_cw_set)
				{	/* This is a duplicate set (update_trans is TRUE, but cw_set is NULL).
					 * OR we hold crit and are exiting. There is nothing to commit to the db.
					 * Now that we finished jnl record writing, break.
					 */
					break;
				}
				if (is_bg)
				{
					clru = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cnl->cur_lru_cache_rec_off);
					lcnt = 0;
				}
				/* Determine transaction number to use for the gvcst_*_build functions.
				 * If not phase2, then we have crit, so it is the same as the current database transaction number.
				 * If phase2, then we don't have crit, so use value stored in "start_tn" or "si->start_tn".
				 */
				if (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit)
				{
					currtn = csd->trans_hist.curr_tn;
					if (dollar_tlevel && (T_COMMIT_CRIT_PHASE0 == csa->t_commit_crit))
					{	/* This region is part of a TP transaction where another region got an error
						 * inside PHASE1 but this region is still in PHASE0 (since this region is
						 * later in the "first_tp_si_by_ftok" list order). Finish CMT08 & CMT09
						 * stages for this region.
						 */
						cnl->update_underway_tn = currtn;	/* Roll forward CMT08 */
						/* Note csa->t_commit_crit = T_COMMIT_CRIT_PHASE1 is also set in CMT08
						 * but we do not do that here as that confuses code below and is anyways
						 * a process-private flag that is not needed otherwise below.
						 */
						if (jnl_fence_ctl.replication && REPL_ALLOWED(csa))
						{	/* Indication that this is an update to a replicated region
							 * that bumps the journal seqno. So finish CMT09.
							 * Note: In "tp_tend", the variable "supplementary" is TRUE if
							 * "jnl_fence_ctl.strm_seqno" is non-zero. We use that here since
							 * the local variable "supplementary" is not available here.
							 */
							strm_seqno = GET_STRM_SEQ60(jnl_fence_ctl.strm_seqno);
#							ifdef DEBUG
							assert(!jnl_fence_ctl.strm_seqno
								|| ((INVALID_SUPPL_STRM != strm_index)
									&& (GET_STRM_INDEX(jnl_fence_ctl.strm_seqno)
													== strm_index)));
							assert(jnlpool.jnlpool_dummy_reg->open);
							repl_csa = REG2CSA(jnlpool.jnlpool_dummy_reg);
							assert(repl_csa->now_crit);
							/* see jnlpool_init() for relationship between critical and jpl */
							jpl = (jnlpool_ctl_ptr_t)((sm_uc_ptr_t)repl_csa->critical
											- JNLPOOL_CTL_SIZE);
							assert(jpl->jnl_seqno == (jnl_fence_ctl.token + 1));
							assert(!jnl_fence_ctl.strm_seqno
									|| (jpl->strm_seqno[strm_index] == (strm_seqno + 1)));
#							endif
							SET_REG_SEQNO(csa, jnl_fence_ctl.token + 1, jnl_fence_ctl.strm_seqno,	\
								strm_index, strm_seqno + 1, SKIP_ASSERT_FALSE); /* Step CMT09 */
						}
					}
				} else
				{
					if (tp_update_underway)		/* otherwise currtn initialized above from start_tn */
						currtn = si->start_tn;
					assert(currtn < csd->trans_hist.curr_tn);
				}
				blk_size = csd->blk_size;
				for ( ; (tp_update_underway && (NULL != cs)) || (!tp_update_underway && (cs < cs_top));
									cs = tp_update_underway ? orig_cs->next_cw_set : (cs + 1))
				{
					donot_reset_data_invalid = FALSE;
					if (tp_update_underway)
					{
						orig_cs = cs;
						TRAVERSE_TO_LATEST_CSE(cs);
					}
					if (gds_t_committed < cs->mode)
					{
						assert(n_gds_t_op != cs->mode);
						if (n_gds_t_op > cs->mode)
						{	/* Currently there are only three possibilities and each is in NON-TP.
							 * In each case, no need to do any block update so simulate commit.
							 */
							assert(!tp_update_underway);
							assert((gds_t_write_root == cs->mode) || (gds_t_busy2free == cs->mode)
									|| (gds_t_recycled2free == cs->mode));
							/* Check if BG AND gds_t_busy2free and if so UNPIN corresponding
							 * cache-record. This needs to be done only if we hold crit as otherwise
							 * it means we have already done it in t_end. But to do this we need to
							 * pass the global variable array "cr_array" from GTM to GTMSECSHR which
							 * is better avoided. Since anyways we have crit at this point, we are
							 * going to set wc_blocked later which is going to trigger cache recovery
							 * that is going to unpin all the cache-records so we don't take the
							 * trouble to do it here.
							 */
						} else
						{	/* Currently there are only two possibilities and both are in TP.
							 * In either case, need to simulate what tp_tend would have done which
							 * is to build a private copy right now if this is the first phase of
							 * KILL (i.e. we hold crit) as this could be needed in the 2nd phase
							 * of KILL.
							 */
							assert(tp_update_underway);
							assert((kill_t_write == cs->mode) || (kill_t_create == cs->mode));
							if (csa->now_crit && !cs->done)
							{
								/* Initialize cs->new_buff to non-NULL since sec_shr_blk_build
								 * expects this.
								 */
								if (NULL == cs->new_buff)
									cs->new_buff = (unsigned char *)
											get_new_free_element(si->new_buff_list);
								assert(NULL != cs->new_buff);
								blk_ptr = (sm_uc_ptr_t)cs->new_buff;
								if (FALSE == sec_shr_blk_build(csa, csd, is_bg, cs, blk_ptr,
												currtn))
								{
									SECSHR_ACCOUNTING(do_accounting, 10);
									SECSHR_ACCOUNTING(do_accounting, __LINE__);
									SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
									SECSHR_ACCOUNTING(do_accounting, cs->blk);
									SECSHR_ACCOUNTING(do_accounting, cs->level);
									SECSHR_ACCOUNTING(do_accounting, cs->done);
									SECSHR_ACCOUNTING(do_accounting, cs->forward_process);
									SECSHR_ACCOUNTING(do_accounting, cs->first_copy);
									SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs->upd_addr);
									SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs->new_buff);
									assert(FALSE);
									continue;
								} else if (cs->ins_off != 0)
								{
									if ((cs->ins_off
										> ((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
										|| (cs->ins_off
										 < (SIZEOF(blk_hdr) + SIZEOF(rec_hdr))))
									{
										SECSHR_ACCOUNTING(do_accounting, 7);
										SECSHR_ACCOUNTING(do_accounting, __LINE__);
										SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
										SECSHR_ACCOUNTING(do_accounting, cs->blk);
										SECSHR_ACCOUNTING(do_accounting, cs->index);
										SECSHR_ACCOUNTING(do_accounting, cs->ins_off);
										SECSHR_ACCOUNTING(do_accounting,	\
											((blk_hdr *)blk_ptr)->bsiz);
										assert(FALSE);
										continue;
									}
									if (cs->first_off == 0)
										cs->first_off = cs->ins_off;
									chain_ptr = blk_ptr + cs->ins_off;
									chain.flag = 1;
									/* note: currently only assert check of cs->index */
									assert(tp_update_underway || (0 <= (short)cs->index));
									assert(tp_update_underway
										|| (&first_cw_set[cs->index] < cs));
									chain.cw_index = cs->index;
									chain.next_off = cs->next_off;
									GET_LONGP(chain_ptr, &chain);
									cs->ins_off = cs->next_off = 0;
								}
								cs->done = TRUE;
								assert(NULL != cs->blk_target);
								CERT_BLK_IF_NEEDED(certify_all_blocks, reg,
											cs, cs->new_buff, ((gv_namehead *)NULL));
							}
						}
						cs->old_mode = (int4)cs->mode;
						assert(0 < cs->old_mode);
						cs->mode = gds_t_committed;
						continue;
					}
					old_mode = cs->old_mode;
					if (gds_t_committed == cs->mode)
					{	/* already processed */
						assert(0 < old_mode);
						if (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit)
						{
							assert(csa->now_crit);
							csd->trans_hist.free_blocks -= cs->reference_cnt;
						}
						if (tp_update_underway)
						{	/* We have seen at least one already-committed cse. Assume GT.M has
							 * committed ALL cses if this is the first one we are seeing. This
							 * will be later overridden if we see an uncommitted cse in this region.
							 * If we have already decided that the region is only partially committed,
							 * do not change that. It is possible to see uncommitted cses followed by
							 * committed cses in case of an error during phase2 because bitmaps
							 * (later cses) are committed in phase1 while the rest (early cses)
							 * are completely committed only in phase2.
							 */
							if (REG_COMMIT_UNSTARTED == this_reg_commit_type)
								this_reg_commit_type = REG_COMMIT_COMPLETE;
						}
						cr = cs->cr;
						assert(!dollar_tlevel || (gds_t_write_root != old_mode));
						assert(gds_t_committed != old_mode);
						if (gds_t_committed > old_mode)
							assert(process_id != cr->in_tend);
						else
						{	/* For the kill_t_* case, cs->cr will be NULL as bg_update was not invoked
							 * and the cw-set-elements were memset to 0 in TP. But for gds_t_write_root
							 * and gds_t_busy2free, they are non-TP ONLY modes and cses are not
							 * initialized so can't check for NULL cr. Thankfully "n_gds_t_op"
							 * demarcates the boundaries between non-TP only and TP only modes.
							 * So use that.
							 */
							assert((n_gds_t_op > old_mode) || (NULL == cr));
						}
						continue;
					}
					/* Since we are going to build blocks at this point, unconditionally set wc_blocked
					 * (after finishing commits) to trigger wcs_recover even though we might not be
					 * holding crit at this point.
					 */
					set_wc_blocked = TRUE;
					/* for normal termination we should not have been in the midst of commit */
					assert(!is_exiting || WBTEST_ENABLED(WBTEST_SLEEP_IN_WCS_WTSTART));
					if (tp_update_underway)
					{	/* Since the current cse has not been committed, this is a partial
						 * GT.M commit in this region even if we have already seen committed cses.
						 */
						this_reg_commit_type = REG_COMMIT_PARTIAL;
					}
					if (is_bg)
					{
						assert((T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit) || (0 > old_mode));
						if (0 <= old_mode)
						{	/* We did not yet finish phase1 of commit for this cs which means we
							 * hold crit on this region, so have to find out a free cache-record
							 * we can dump our updates onto.
							 */
							for ( ; lcnt++ < max_bts; )
							{	/* find any available cr */
								if (++clru >= cr_top)
									clru = start_cr;
								assert(!clru->stopped);
								if (!clru->stopped && (0 == clru->dirty)
										&& (0 == clru->in_cw_set)
										&& (!clru->in_tend)
										&& (-1 == clru->read_in_progress))
									break;
							}
							if (lcnt >= max_bts)
							{	/* Did not find space in global buffers to finish commit */
								SECSHR_ACCOUNTING(do_accounting, 9);
								SECSHR_ACCOUNTING(do_accounting, __LINE__);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
								SECSHR_ACCOUNTING(do_accounting, cs->blk);
								SECSHR_ACCOUNTING(do_accounting, cs->tn);
								SECSHR_ACCOUNTING(do_accounting, cs->level);
								SECSHR_ACCOUNTING(do_accounting, cs->done);
								SECSHR_ACCOUNTING(do_accounting, cs->forward_process);
								SECSHR_ACCOUNTING(do_accounting, cs->first_copy);
								assert(FALSE);
								continue;
							}
							cr = clru;
							cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
							assert(cs->blk < csd->trans_hist.total_blks);
							cr->blk = cs->blk;
							assert(CR_BLKEMPTY != cr->blk);
							cr->jnl_addr = cs->jnl_freeaddr;
							assert(!cr->twin);
							cr->stopped = process_id;
							/* Keep cs->cr and t1->cr uptodate to ensure clue will be accurate */
							cs->cr = cr;
							cs->cycle = cr->cycle;
							if (!IS_BITMAP_BLK(cs->blk))
							{	/* Not a bitmap block, update clue history to reflect new cr */
								assert((0 <= cs->level) && (MAX_BT_DEPTH > cs->level));
								gvtarget = cs->blk_target;
								assert((MAX_BT_DEPTH + 1) == ARRAYSIZE(gvtarget->hist.h));
								if ((0 <= cs->level) && (MAX_BT_DEPTH > cs->level)
									&& (NULL != gvtarget)
									&& (0 != gvtarget->clue.end))
								{
									t1 = &gvtarget->hist.h[cs->level];
									if (t1->blk_num == cs->blk)
									{
										t1->cr = cr;
										t1->cycle = cs->cycle;
										t1->buffaddr = (sm_uc_ptr_t)
												GDS_ANY_REL2ABS(csa, cr->buffaddr);
									}
								}
							}
						} else
						{	/* "old_mode < 0" implies we are done PHASE1 of the commit for this cs
							 * and have already picked out a cr for the commit but did not finish
							 * phase2 yet. Use the picked out "cr" and finish phase2.
							 */
							cr = cs->cr;
							assert(process_id == cr->in_tend);
							assert(process_id == cr->in_cw_set);
							assert(cr->blk == cs->cr->blk);
							if (cr->data_invalid)
							{	/* Buffer is already in middle of update. Since blk builds are
								 * not redoable, db is in danger whether or not we redo the build.
								 * Since, skipping the build is guaranteed to give us integrity
								 * errors, we redo the build hoping it will have at least a 50%
								 * chance of resulting in a clean block. Make sure data_invalid
								 * flag is set until the next cache-recovery (wcs_recover will
								 * send a DBDANGER syslog message for this block to alert of
								 * potential database damage) by setting donot_reset_data_invalid.
								 */
								SECSHR_ACCOUNTING(do_accounting, 6);
								SECSHR_ACCOUNTING(do_accounting, __LINE__);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cr);
								SECSHR_ACCOUNTING(do_accounting, cr->blk);
								SECSHR_ACCOUNTING(do_accounting, cr->data_invalid);
								assert(FALSE);
								donot_reset_data_invalid = TRUE;
							}
						}
						/* Check if online backup is in progress and if there is a before-image to write.
						 * If so need to store link to it so wcs_recover can back it up later. Cannot
						 * rely on precomputed value csa->backup_in_prog since it is not initialized
						 * if (cw_depth == 0) (see t_end.c). Hence using cnl->nbb explicitly in check.
						 * However, for snapshots we can rely on csa as it is computed under
						 * if (update_trans). Use cs->blk_prior_state's free status to ensure that FREE
						 * blocks are not back'ed up either by secshr_db_clnup or wcs_recover.
						 */
						if ((SNAPSHOTS_IN_PROG(csa) || (BACKUP_NOT_IN_PROGRESS != cnl->nbb))
												&& (NULL != cs->old_block))
						{
							/* If online backup is concurrently running, backup the block here */
							blk_hdr_ptr = (blk_hdr_ptr_t)cs->old_block;
							ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)blk_hdr_ptr, csa);
							bufindx = ((sm_uc_ptr_t)blk_hdr_ptr - bufstart) / blk_size;
							assert(0 <= bufindx);
							assert(bufindx < csd->n_bts);
							cr_alt = &start_cr[bufindx];
							assert(!cr->stopped || (cr != cr_alt));
							/* The following check is similar to the one in BG_BACKUP_BLOCK */
							if (!WAS_FREE(cs->blk_prior_state) && (cr_alt->blk >= cnl->nbb)
								&& (0 == csa->shmpool_buffer->failed)
								&& (blk_hdr_ptr->tn < csa->shmpool_buffer->backup_tn)
								&& (blk_hdr_ptr->tn >= csa->shmpool_buffer->inc_backup_tn))
							{
								assert(cr->stopped || (cr == cr_alt));
								backup_block(csa, cr_alt->blk, cr_alt, NULL);
								/* No need for us to flush the backup buffer.
								 * MUPIP BACKUP will anyways flush it at the end.
								 */
							}
							if (SNAPSHOTS_IN_PROG(csa))
							{
								lcl_ss_ctx = SS_CTX_CAST(csa->ss_ctx);
								snapshot_cr = cr_alt;
								if (snapshot_cr->blk < lcl_ss_ctx->total_blks)
									WRITE_SNAPSHOT_BLOCK(csa, snapshot_cr, NULL,
												snapshot_cr->blk, lcl_ss_ctx);
							}
						}
						if (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit)
						{	/* Adjust blks_to_upgrd counter if not already done in phase1. The value of
							 * cs->old_mode if negative implies phase1 is complete on this cse so we
							 * don't need to do this adjustment again. If not we do the adjustment.
							 */
							assert((0 <= cs->old_mode) || (cs->old_mode == -cs->mode));
							if (0 <= cs->old_mode)
							{	/* the following code is very similar to that in bg_update */
								if (gds_t_acquired == cs->mode)
								{
									if (GDSV4 == csd->desired_db_format)
										INCR_BLKS_TO_UPGRD(csa, csd, 1);
								} else
								{
#									ifdef DEBUG
									/* secshr_db_clnup relies on the fact that cs->ondsk_blkver
									 * accurately reflects the on-disk block version of the
									 * block and therefore can be used to set cr->ondsk_blkver.
									 * Confirm this by checking that if a cr exists for this
									 * block, then that cr's ondsk_blkver matches with the cs.
									 * db_csh_get uses the global variable cs_addrs to determine
									 * the region. So make it uptodate temporarily holding its
									 * value in the local variable tmp_csa.
									 */
									tmp_csa = cs_addrs;	/* save cs_addrs in local */
									cs_addrs = csa;		/* set cs_addrs for db_csh_get */
									actual_cr = db_csh_get(cs->blk);
									cs_addrs = tmp_csa;	/* restore cs_addrs */
									/* actual_cr can be NULL if the block is NOT in the cache.
									 * It can be CR_NOTVALID if the cache record originally
									 * containing this block got reused for a different block
									 * (i.e. cr->stopped = non-zero) as part of secshr_db_clnup.
									 */
									assert((NULL == actual_cr)
										|| ((cache_rec_ptr_t)CR_NOTVALID == actual_cr)
										|| (cs->ondsk_blkver == actual_cr->ondsk_blkver));
#									endif
									cr->ondsk_blkver = cs->ondsk_blkver;
									if (cr->ondsk_blkver != csd->desired_db_format)
									{
										if (GDSV4 == csd->desired_db_format)
										{
											if (gds_t_write_recycled != cs->mode)
												INCR_BLKS_TO_UPGRD(csa, csd, 1);
										} else
										{
											if (gds_t_write_recycled != cs->mode)
												DECR_BLKS_TO_UPGRD(csa, csd, 1);
										}
									}
								}
							}
						}
						/* Before resetting cr->ondsk_blkver, ensure db_format in file header did not
						 * change in between phase1 (inside of crit) and phase2 (outside of crit).
						 * This is needed to ensure the correctness of the blks_to_upgrd counter.
						 */
						assert(currtn > csd->desired_db_format_tn);
						cr->ondsk_blkver = csd->desired_db_format;
						/* else we are in phase2 and all blks_to_upgrd manipulation is already done */
						blk_ptr = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
					} else
					{	/* access method is MM */
						blk_ptr = MM_BASE_ADDR(csa) + (off_t)blk_size * cs->blk;
					}
					/* The following block of code rolls forward Step (CMT10) and/or Step (CMT18) */
					if (cs->mode == gds_t_writemap)
					{
						memmove(blk_ptr, cs->old_block, blk_size);
						if (FALSE == sec_shr_map_build(csa, (uint4*)cs->upd_addr, blk_ptr, cs, currtn))
						{
							SECSHR_ACCOUNTING(do_accounting, 11);
							SECSHR_ACCOUNTING(do_accounting, __LINE__);
							SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
							SECSHR_ACCOUNTING(do_accounting, cs->blk);
							SECSHR_ACCOUNTING(do_accounting, cs->tn);
							SECSHR_ACCOUNTING(do_accounting, cs->level);
							SECSHR_ACCOUNTING(do_accounting, cs->done);
							SECSHR_ACCOUNTING(do_accounting, cs->forward_process);
							SECSHR_ACCOUNTING(do_accounting, cs->first_copy);
							SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs->upd_addr);
							SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)blk_ptr);
							assert(FALSE);
							continue;
						}
					} else
					{
						if (!tp_update_underway)
						{
							if (FALSE == sec_shr_blk_build(csa, csd, is_bg, cs, blk_ptr, currtn))
							{
								SECSHR_ACCOUNTING(do_accounting, 10);
								SECSHR_ACCOUNTING(do_accounting, __LINE__);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
								SECSHR_ACCOUNTING(do_accounting, cs->blk);
								SECSHR_ACCOUNTING(do_accounting, cs->level);
								SECSHR_ACCOUNTING(do_accounting, cs->done);
								SECSHR_ACCOUNTING(do_accounting, cs->forward_process);
								SECSHR_ACCOUNTING(do_accounting, cs->first_copy);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs->upd_addr);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)blk_ptr);
								assert(FALSE);
								continue;
							} else if (cs->ins_off)
							{
								if ((cs->ins_off >
									((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
									|| (cs->ins_off < (SIZEOF(blk_hdr)
										+ SIZEOF(rec_hdr)))
									|| (0 > (short)cs->index)
									|| ((cs - cw_set) <= cs->index))
								{
									SECSHR_ACCOUNTING(do_accounting, 7);
									SECSHR_ACCOUNTING(do_accounting, __LINE__);
									SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
									SECSHR_ACCOUNTING(do_accounting, cs->blk);
									SECSHR_ACCOUNTING(do_accounting, cs->index);
									SECSHR_ACCOUNTING(do_accounting, cs->ins_off);
									SECSHR_ACCOUNTING(do_accounting,		\
											((blk_hdr *)blk_ptr)->bsiz);
									assert(FALSE);
									continue;
								}
								PUT_LONG((blk_ptr + cs->ins_off),
									((cw_set_element *)(cw_set + cs->index))->blk);
								if (((nxt = cs + 1) < cs_top)
									&& (gds_t_write_root == nxt->mode))
								{
									if ((nxt->ins_off >
									     ((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
										|| (nxt->ins_off < (SIZEOF(blk_hdr)
											 + SIZEOF(rec_hdr)))
										|| (0 > (short)nxt->index)
										|| ((cs - cw_set) <= nxt->index))
									{
										SECSHR_ACCOUNTING(do_accounting, 7);
										SECSHR_ACCOUNTING(do_accounting, __LINE__);
										SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)nxt);
										SECSHR_ACCOUNTING(do_accounting, cs->blk);
										SECSHR_ACCOUNTING(do_accounting, nxt->index);
										SECSHR_ACCOUNTING(do_accounting, nxt->ins_off);
										SECSHR_ACCOUNTING(do_accounting,
											((blk_hdr *)blk_ptr)->bsiz);
										assert(FALSE);
										continue;
									}
									PUT_LONG((blk_ptr + nxt->ins_off),
										 ((cw_set_element *)
										 (cw_set + nxt->index))->blk);
								}
							}
						} else
						{	/* TP */
							if (cs->done == 0)
							{
								if (FALSE == sec_shr_blk_build(csa, csd, is_bg, cs, blk_ptr,
												currtn))
								{
									SECSHR_ACCOUNTING(do_accounting, 10);
									SECSHR_ACCOUNTING(do_accounting, __LINE__);
									SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
									SECSHR_ACCOUNTING(do_accounting, cs->blk);
									SECSHR_ACCOUNTING(do_accounting, cs->level);
									SECSHR_ACCOUNTING(do_accounting, cs->done);
									SECSHR_ACCOUNTING(do_accounting, cs->forward_process);
									SECSHR_ACCOUNTING(do_accounting, cs->first_copy);
									SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs->upd_addr);
									SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)blk_ptr);
									assert(FALSE);
									continue;
								}
								if (cs->ins_off != 0)
								{
									if ((cs->ins_off
										> ((blk_hdr *)blk_ptr)->bsiz
											- SIZEOF(block_id))
										|| (cs->ins_off
										 < (SIZEOF(blk_hdr) + SIZEOF(rec_hdr))))
									{
										SECSHR_ACCOUNTING(do_accounting, 7);
										SECSHR_ACCOUNTING(do_accounting, __LINE__);
										SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
										SECSHR_ACCOUNTING(do_accounting, cs->blk);
										SECSHR_ACCOUNTING(do_accounting, cs->index);
										SECSHR_ACCOUNTING(do_accounting, cs->ins_off);
										SECSHR_ACCOUNTING(do_accounting,	\
											((blk_hdr *)blk_ptr)->bsiz);
										assert(FALSE);
										continue;
									}
									if (cs->first_off == 0)
										cs->first_off = cs->ins_off;
									chain_ptr = blk_ptr + cs->ins_off;
									chain.flag = 1;
									chain.cw_index = cs->index;
									/* note: currently no verification of cs->index */
									chain.next_off = cs->next_off;
									GET_LONGP(chain_ptr, &chain);
									cs->ins_off = cs->next_off = 0;
								}
							} else
							{
								memmove(blk_ptr, cs->new_buff,
									((blk_hdr *)cs->new_buff)->bsiz);
								((blk_hdr *)blk_ptr)->tn = currtn;
							}
							if (cs->first_off)
							{
								for (chain_ptr = blk_ptr + cs->first_off; ;
									chain_ptr += chain.next_off)
								{
									GET_LONGP(&chain, chain_ptr);
									if ((1 == chain.flag)
										&& ((chain_ptr - blk_ptr + SIZEOF(block_id))
											<= ((blk_hdr *)blk_ptr)->bsiz)
										&& (chain.cw_index < si->cw_set_depth))
									{
										save_si = sgm_info_ptr;
										sgm_info_ptr = si;	/* needed by "tp_get_cw" */
										tp_get_cw(first_cw_set, chain.cw_index, &cs_ptr);
										sgm_info_ptr = save_si;
										PUT_LONG(chain_ptr, cs_ptr->blk);
										if (0 == chain.next_off)
											break;
									} else
									{
										SECSHR_ACCOUNTING(do_accounting, 11);
										SECSHR_ACCOUNTING(do_accounting, __LINE__);
										SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cs);
										SECSHR_ACCOUNTING(do_accounting, cs->blk);
										SECSHR_ACCOUNTING(do_accounting, cs->index);
										SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)blk_ptr);
										SECSHR_ACCOUNTING(do_accounting,	\
												(INTPTR_T)chain_ptr);
										SECSHR_ACCOUNTING(do_accounting, chain.next_off);
										SECSHR_ACCOUNTING(do_accounting, chain.cw_index);
										SECSHR_ACCOUNTING(do_accounting, si->cw_set_depth);
										SECSHR_ACCOUNTING(do_accounting,	\
											((blk_hdr *)blk_ptr)->bsiz);
										assert(FALSE);
										break;
									}
								}
							}
						}	/* TP */
					}	/* non-map processing */
					if (0 > cs->reference_cnt)
					{	/* blocks were freed up */
						assert(non_tp_update_underway);
						assert(((inctn_bmp_mark_free_gtm == inctn_opcode)
								|| (inctn_bmp_mark_free_mu_reorg == inctn_opcode)
								|| (inctn_blkmarkfree == inctn_opcode)
								|| dse_running));
						/* Check if we are freeing a V4 format block and if so decrement the
						 * blks_to_upgrd counter. Do not do this in case MUPIP REORG UPGRADE/DOWNGRADE
						 * is marking a recycled block as free (inctn_opcode is inctn_blkmarkfree).
						 */
						if (((inctn_bmp_mark_free_gtm == inctn_opcode)
								|| (inctn_bmp_mark_free_mu_reorg == inctn_opcode))
								&& (0 != inctn_detail.blknum_struct.blknum))
							DECR_BLKS_TO_UPGRD(csa, csd, 1);
					}
					assert(!cs->reference_cnt || (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit));
					if (csa->now_crit)
					{	/* Even though we know cs->reference_cnt is guaranteed to be 0 if we are in
						 * phase2 of commit (see above assert), we still do not want to be touching
						 * free_blocks in the file header outside of crit as it could potentially
						 * result in an incorrect value of the free_blocks counter. This is because
						 * in between the time we note down the current value of free_blocks on the
						 * right hand side of the below expression and assign the same value to the
						 * left side, it is possible that a concurrent process holding crit could
						 * have updated the free_blocks counter. In that case, our update would
						 * result in incorrect values. Hence don't touch this field if phase2.
						 */
						csd->trans_hist.free_blocks -= cs->reference_cnt;
					}
					cs->old_mode = (int4)cs->mode;
					assert(0 < cs->old_mode);
					cs->mode = gds_t_committed;	/* rolls forward Step (CMT18) */
					/* Do not do a cert_blk of bitmap here since it could give a DBBMMSTR error. The
					 * bitmap block build is COMPLETE only in wcs_recover so do the cert_blk there.
					 * Assert that the bitmap buffer will indeed go through cert_blk there.
					 */
					CERT_BLK_IF_NEEDED(certify_all_blocks, reg, cs, blk_ptr, ((gv_namehead *)NULL));
					if (is_bg && (process_id == cr->in_tend))
					{	/* Reset cr->in_tend now that cr is uptodate.
						 * Take this opportunity to reset data_invalid, in_cw_set and the write interlock
						 * as well thereby simulating exactly what bg_update_phase2 would have done.
						 */
						if (!donot_reset_data_invalid)
							cr->data_invalid = 0;
						/* Release write interlock. The following code is very similar to that
						 * at the end of the function "bg_update_phase2".
						 */
						/* Avoid using gv_cur_region in the LOCK_HIST macro that is
						 * used by the RELEASE_BUFF_UPDATE_LOCK macro by setting locknl
						 */
						locknl = cnl;
						if (!cr->tn)
						{
							cr->jnl_addr = cs->jnl_freeaddr;
							assert(LATCH_SET == WRITE_LATCH_VAL(cr));
							/* cache-record was not dirty BEFORE this update.
							 * insert this in the active queue.
							 */
							n = INSQTI((que_ent_ptr_t)&cr->state_que,
									(que_head_ptr_t)&cache_state->cacheq_active);
							if (INTERLOCK_FAIL == n)
							{
								SECSHR_ACCOUNTING(do_accounting, 7);
								SECSHR_ACCOUNTING(do_accounting, __LINE__);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cr);
								SECSHR_ACCOUNTING(do_accounting, cr->blk);
								SECSHR_ACCOUNTING(do_accounting, n);
								SECSHR_ACCOUNTING(do_accounting, cache_state->cacheq_active.fl);
								SECSHR_ACCOUNTING(do_accounting, cache_state->cacheq_active.bl);
								assert(FALSE);
							}
							ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
						}
						RELEASE_BUFF_UPDATE_LOCK(cr, n, &cnl->db_latch);
						/* "n" holds the pre-release value so check that we did hold the
						 * lock before releasing it above.
						 */
						assert(LATCH_CONFLICT >= n);
						assert(LATCH_CLEAR < n);
						if (WRITER_BLOCKED_BY_PROC(n))
						{
							n = INSQHI((que_ent_ptr_t)&cr->state_que,
									(que_head_ptr_t)&cache_state->cacheq_active);
							if (INTERLOCK_FAIL == n)
							{
								SECSHR_ACCOUNTING(do_accounting, 7);
								SECSHR_ACCOUNTING(do_accounting, __LINE__);
								SECSHR_ACCOUNTING(do_accounting, (INTPTR_T)cr);
								SECSHR_ACCOUNTING(do_accounting, cr->blk);
								SECSHR_ACCOUNTING(do_accounting, n);
								SECSHR_ACCOUNTING(do_accounting, cache_state->cacheq_active.fl);
								SECSHR_ACCOUNTING(do_accounting, cache_state->cacheq_active.bl);
								assert(FALSE);
							}
						}
						RESET_CR_IN_TEND_AFTER_PHASE2_COMMIT(cr); /* resets cr->in_tend & cr->in_cw_set
											   * (for older twin too if needed).
											   */
					}
				}	/* for all cw_set entries */
#				ifdef DEBUG
				/* If we still hold crit, then we got a commit-time error in bg_update_phase1 which means we
				 * would have pinned crs in t_end/tp_tend but would not have unpinned those here but instead
				 * dumped the commits onto crs with cr->stopped a non-zero value. Accessing the original pinned
				 * cr requires us to have access to the histories which are available in case of TP (through si)
				 * but not easily in case of non-tp since those are passed as a parameter to t_end (and not
				 * otherwise available through a global variable). Since we hold crit and we are going to anyways
				 * set cnl->wc_blocked to TRUE a little later, the next process to get crit will run "wcs_recover"
				 * which will take care of clearing the "in_cw_set". So no need to do the unpin here in that case.
				 * On the other hand, if we don't have crit, we are done with phase1 of commit which means
				 * all commits would go to crs that we have access to (through cs->cr) and so we should be able
				 * to unpin exactly all of them. So assert accordingly below.
				 */
				if (!csa->now_crit)
				{
					if (tp_update_underway)
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
#				endif
				/* Check if kill_in_prog flag in file header has to be incremented. */
				if (tp_update_underway)
				{	/* TP : Do this only if GT.M has not already completed the commit on this region. */
					assert((REG_COMMIT_COMPLETE == this_reg_commit_type)
						|| (REG_COMMIT_PARTIAL == this_reg_commit_type)
						|| (REG_COMMIT_UNSTARTED == this_reg_commit_type));
					si->cr_array_index = 0; /* Take this opportunity to reset si->cr_array_index */
					if (REG_COMMIT_COMPLETE != this_reg_commit_type)
					{
						if ((NULL != si->kill_set_head) && (NULL == si->kip_csa))
							INCR_KIP(csd, csa, si->kip_csa);
					} else
						assert((NULL == si->kill_set_head) || (NULL != si->kip_csa));
					assert((NULL == si->kill_set_head) || (NULL != si->kip_csa));
				} else
				{	/* Non-TP. Check need_kip_incr and value pointed to by kip_csa. */
					assert(non_tp_update_underway);
					cr_array_index = 0; /* Take this opportunity to reset cr_array_index */
					/* Note that kip_csa could be NULL if we are in the
					 * 1st phase of the M-kill and NON NULL if we are in the 2nd phase of the kill.
					 * Only if it is NULL, should we increment the kill_in_prog flag.
					 */
					if (need_kip_incr && (NULL == kip_csa))
					{
						INCR_KIP(csd, csa, kip_csa);
						need_kip_incr = FALSE;
					}
					if (MUSWP_INCR_ROOT_CYCLE == TREF(in_mu_swap_root_state))
						cnl->root_search_cycle++;
				}
			} while (FALSE);
			/* If the process is about to exit AND any kills are in progress (bitmap freeup phase of kill), mark
			 * kill_in_prog as abandoned. Non-TP and TP maintain kill_in_prog information in different structures
			 * so access them appropriately. Note that even for a TP transaction, the bitmap freeup happens as a
			 * non-TP transaction so checking dollar_tlevel is not enough to determine if we are in TP or non-TP.
			 * Thankfully first_sgm_info is guaranteed to be non-NULL in the case of a TP transaction that is
			 * temporarily running its bitmap freeup phase as a non-TP transaction. And for true non-TP
			 * transactions, first_sgm_info is guaranteed to be NULL. So we use this for the determination.
			 */
			if (is_exiting)
			{
				if (NULL != first_sgm_info)
				{
					si = csa->sgm_info_ptr;
					/* Note that it is possible "si" is NULL in case of a GTM-E-MEMORY error in gvcst_init.
					 * Handle that accordingly in the code below.
					 */
					/* Since the kill process cannot be completed, we need to decerement KIP count
					 * and increment the abandoned_kills count.
					 */
					if ((NULL != si->kill_set_head) && (NULL != si->kip_csa))
					{
						assert(csa == si->kip_csa);
						DECR_KIP(csd, csa, si->kip_csa);
						INCR_ABANDONED_KILLS(csd, csa);
					} else
						assert((NULL == si) || (NULL == si->kill_set_head) || (NULL == si->kip_csa));
				} else if (!dollar_tlevel)
				{
					if ((NULL != kip_csa) && (csa == kip_csa))
					{
						assert(0 < kip_csa->hdr->kill_in_prog);
						DECR_KIP(csd, csa, kip_csa);
						INCR_ABANDONED_KILLS(csd, csa);
					}
				}
			}
			if (JNL_ENABLED(csd) && is_exiting)
			{
				jbp = csa->jnl->jnl_buff;
				RELEASE_LATCH_IF_OWNER_AND_EXITING(&jbp->fsync_in_prog_latch, is_exiting);
				if (jbp->io_in_prog_latch.u.parts.latch_pid == process_id)
					RELEASE_SWAPLOCK(&jbp->io_in_prog_latch);
				if (jbp->blocked == process_id)
				{
					assert(csa->now_crit);
					jbp->blocked = 0;
				}
			}
			if (is_exiting && csa->freeze && csd->freeze == process_id && !csa->persistent_freeze)
			{
				csd->image_count = 0;
				csd->freeze = 0;
			}
			if (is_bg && (csa->wbuf_dqd || csa->now_crit || csa->t_commit_crit || set_wc_blocked))
			{	/* if csa->wbuf_dqd == TRUE, most likely failed during REMQHI in wcs_wtstart
				 * 	or db_csh_get.  cache corruption is suspected so set wc_blocked.
				 * if csa->now_crit is TRUE, someone else should clean the cache, so set wc_blocked.
				 * if csa->t_commit_crit is TRUE, even if csa->now_crit is FALSE, we might need cache
				 * 	cleanup (e.g. cleanup of orphaned cnl->wcs_phase2_commit_pidcnt counter in case
				 * 	a process gets shot in the midst of DECR_WCS_PHASE2_COMMIT_PIDCNT macro before
				 * 	decrementing the shared counter but after committing the transaction otherwise)
				 * 	so set wc_blocked. This case is folded into phase2 cleanup case below.
				 * if set_wc_blocked is TRUE, need to clean up queues after phase2 commits.
				 */
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				if (csa->now_crit)
				{
					wcblocked_ptr = WCBLOCKED_NOW_CRIT_LIT;
					BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_now_crit);
				} else if (csa->wbuf_dqd)
				{
					wcblocked_ptr = WCBLOCKED_WBUF_DQD_LIT;
					BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_wbuf_dqd);
				} else
				{
					wcblocked_ptr = WCBLOCKED_PHASE2_CLNUP_LIT;
					BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_phase2_clnup);
				}
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_STR(wcblocked_ptr),
					process_id, &csd->trans_hist.curr_tn, DB_LEN_STR(reg));
			}
			csa->wbuf_dqd = 0;		/* We can clear the flag now */
			if (csa->wcs_pidcnt_incremented)
				DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
			if (csa->now_crit)
			{
				if (csd->trans_hist.curr_tn == csd->trans_hist.early_tn - 1)
				{	/* there can be at most one region in non-TP with different curr_tn and early_tn */
					assert(!non_tp_update_underway || first_time);
					/* for normal termination we should not have been in the midst of commit */
					assert(!is_exiting || WBTEST_ENABLED(WBTEST_JNL_CREATE_FAIL)
						|| WBTEST_ENABLED(WBTEST_SLEEP_IN_WCS_WTSTART)
						|| (!update_underway && in_wcs_recover));
					DEBUG_ONLY(first_time = FALSE;)
					if (update_underway)
						INCREMENT_CURR_TN(csd);	/* roll forward Step (CMT12) */
					else
						csd->trans_hist.early_tn = csd->trans_hist.curr_tn;
				}
				assert(csd->trans_hist.early_tn == csd->trans_hist.curr_tn);
				/* ONLINE ROLLBACK can come here holding crit ONLY due to commit errors but NOT during
				 * process exiting as secshr_db_clnup during process exiting is always preceded by
				 * mur_close_files which does the rel_crit anyways. Assert that.
				 */
				assert(!csa->hold_onto_crit || !jgbl.onlnrlbk || !is_exiting);
				if (!csa->hold_onto_crit || is_exiting)
				{ 	/* Release crit but since it involves modifying more than one field, make sure
					 * we prevent interrupts while in this code. The global variable "crit_count"
					 * does this for us. See similar usage in rel_crit.c.
					 */
					assert(0 == crit_count);
					crit_count++;	/* prevent interrupts */
					CRIT_TRACE(crit_ops_rw); /* see gdsbt.h for comment on placement */
					if (cnl->in_crit == process_id)
						cnl->in_crit = 0;
					csa->hold_onto_crit = FALSE;
					DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
					mutex_unlockw(reg, crash_count);/* roll forward Step (CMT14) */
					assert(!csa->now_crit);
					DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
					crit_count = 0;
					UNSUPPORTED_PLATFORM_CHECK;
				}
			}
			csa->t_commit_crit = FALSE; /* ensure we don't process this region again (rolls forward Step (CMT19)) */
			if (is_exiting && (NULL != csa->shmpool_buffer))
			{
				if ((pid = csa->shmpool_buffer->shmpool_crit_latch.u.parts.latch_pid) == process_id)
				{
					if (is_exiting)
					{	/* Tiz our lock. Force recovery to run and release */
						csa->shmpool_buffer->shmpool_blocked = TRUE;
						BG_TRACE_PRO_ANY(csa, shmpool_blkd_by_sdc);
						SET_LATCH_GLOBAL(&csa->shmpool_buffer->shmpool_crit_latch, LOCK_AVAILABLE);
						DEBUG_LATCH(util_out_print("Latch cleaned up", FLUSH));
					}
				} else if (0 != pid && FALSE == is_proc_alive(pid, 0))
				{
					/* Attempt to make it our lock so we can set blocked */
					if (COMPSWAP_LOCK(&csa->shmpool_buffer->shmpool_crit_latch, pid, 0, process_id, 0))
					{	/* Now our lock .. set blocked and release.  */
						csa->shmpool_buffer->shmpool_blocked = TRUE;
						BG_TRACE_PRO_ANY(csa, shmpool_blkd_by_sdc);
						DEBUG_LATCH(util_out_print("Orphaned latch cleaned up", TRUE));
						COMPSWAP_UNLOCK(&csa->shmpool_buffer->shmpool_crit_latch, process_id,
								0, LOCK_AVAILABLE, 0);
					} /* Else someone else took care of it */
				}
			}
			/* All releases done now. Double check latch is really cleared */
			assert(NULL != csa->critical);
			/* as long as csa->hold_onto_crit is FALSE, we should have released crit if we held it at entry */
			assert(!csa->now_crit || csa->hold_onto_crit);
			/* Note: Do not release crit if we still hold it. As we want the next process to grab crit to invoke
			 * "mutex_salvage" (to cleanup stuff) in case we terminate while holding crit. Hence the below line
			 * is commented out.
			 *
			 * RELEASE_LATCH_IF_OWNER_AND_EXITING(&csa->critical->semaphore, is_exiting);
			 */
			RELEASE_LATCH_IF_OWNER_AND_EXITING(&csa->critical->crashcnt_latch, is_exiting);
			RELEASE_LATCH_IF_OWNER_AND_EXITING(&csa->critical->prochead.latch, is_exiting);
			RELEASE_LATCH_IF_OWNER_AND_EXITING(&csa->critical->freehead.latch, is_exiting);
		}	/* For all regions */
	}	/* For all glds */
	if ((NULL != (reg = jnlpool.jnlpool_dummy_reg)) && reg->open)
	{
		csa = REG2CSA(reg);
		if (csa->now_crit)
		{
			jpl = REPLCSA2JPL(csa);
			cnl = csa->nl;
			/* ONLINE ROLLBACK can come here holding crit ONLY due to commit errors but NOT during
			 * process exiting as secshr_db_clnup during process exiting is always preceded by
			 * mur_close_files which does the rel_crit anyways. Assert that.
			 */
			assert(!csa->hold_onto_crit || !jgbl.onlnrlbk || !is_exiting);
			if (!csa->hold_onto_crit || is_exiting)
			{
				CRIT_TRACE(crit_ops_rw); /* see gdsbt.h for comment on placement */
				if (cnl->in_crit == process_id)
					cnl->in_crit = 0;
				csa->hold_onto_crit = FALSE;
				DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
				mutex_unlockw(reg, 0);		/* roll forward Step (CMT15) */
				assert(!csa->now_crit);
				DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
			}
		}
		/* as long as csa->hold_onto_crit is FALSE, we should have released crit if we held it at entry */
		assert(!csa->now_crit || csa->hold_onto_crit);
		assert(jnlpool.jnlpool_dummy_reg == reg);
		FINISH_JNL_PHASE2_IN_JNLPOOL_IF_NEEDED(TRUE, jnlpool);	/* Roll forward CMT17 */
	}
	if (is_exiting)
	{	/* It is possible we are exiting while in the middle of a transaction (e.g. called through "deferred_signal_handler"
		 * in the DEFERRED_EXIT_HANDLING_CHECK macro). Since exit handling code can start new non-TP transactions
		 * (e.g. for statsdb rundown you need to kill ^%YGS node, for recording mprof stats you need to set a global node)
		 * clean up the effects of any in-progress transaction before the "t_begin" call happens.
		 */
		if (!dollar_tlevel)
		{
			assert(save_gv_cur_region == gv_cur_region);
			if ((NULL != save_gv_cur_region) && save_gv_cur_region->open && IS_REG_BG_OR_MM(save_gv_cur_region))
				t_abort(save_gv_cur_region, &FILE_INFO(save_gv_cur_region)->s_addrs);
		} else
			OP_TROLLBACK(0);
	}
	return;
}
