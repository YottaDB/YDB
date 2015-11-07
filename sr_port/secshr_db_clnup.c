/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <ssdef.h>
#include <psldef.h>
#include <descrip.h>
#endif

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
#ifdef VMS
#include "lockconst.h"
#endif
#include "interlock.h"
#include "jnl.h"
#include "probe.h"
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
#ifdef UNIX
#include "mutex.h"
#include "repl_instance.h"	/* needed for JNLDATA_BASE_OFF macro */
#include "mupipbckup.h"		/* needed for backup_block prototype */
#include "cert_blk.h"		/* for CERT_BLK_IF_NEEDED macro */
#include "relqueopi.h"		/* for INSQTI and INSQHI macros */
#include "caller_id.h"
#endif
#include "sec_shr_blk_build.h"
#include "sec_shr_map_build.h"
#include "add_inter.h"
#include "send_msg.h"	/* for send_msg prototype */
#include "secshr_db_clnup.h"
#include "gdsbgtr.h"
#include "memcoherency.h"
#include "shmpool.h"
#include "wbox_test_init.h"
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif
#include "muextr.h"
#include "mupip_reorg.h"

/* This section documents DOs and DONTs about code used by GTMSECSHR on Alpha VMS. Any module linked into GTMSECSHR (see
 * secshrlink.axp for the current list) must follow certain rules as GTMSECSHR provides user-defined system services
 * (privileged image that runs in kernel mode). See "Creating User Written System Sevice" chapter of the "Programming Concepts"
 * OpenVMS manual and the "Shareable Images Cookbook" available from the OpenVMS Wizard's page. SYS$EXAMPLES:uwss*.* is also a
 * good reference.
 *
 ** DO NOT use modulo (%) operation. If % is used, GTMSECSHR links with LIBOTS.EXE - an external shared image. This will result
 *  in "-SYSTEM-F-NOSHRIMG, privileged shareable image cannot have outbound calls" errors when GTMSECSHR is invoked. We might as
 *  well avoid division too.
 *
 ** The only library/system calls allowed are SYS$ calls.
 *
 ** No I/O allowed - any device, including operator console.
 *
 ** Always PROBE memory before accessing it. If not, should SECSHR access invalid memory (out of bounds for instance) the machine
 *  will crash (BUGCHECK in VMS parlance). Remember, SECSHR is running in kernel mode!
 *
 ** Both secshr_db_clnup.c and sec_shr_blk_build.c are compiled with /prefix=except=memmove. If any of the other modules used
 *  memmove, they would need special treatment as well.
 */

#define FLUSH 1

#define	WCBLOCKED_NOW_CRIT_LIT		"wcb_secshr_db_clnup_now_crit"
#define	WCBLOCKED_WBUF_DQD_LIT		"wcb_secshr_db_clnup_wbuf_dqd"
#define	WCBLOCKED_PHASE2_CLNUP_LIT	"wcb_secshr_db_clnup_phase2_clnup"

/* IMPORTANT : SECSHR_PROBE_REGION sets csa */
#define	SECSHR_PROBE_REGION(reg)									\
	if (!GTM_PROBE(SIZEOF(gd_region), (reg), READ))							\
		continue; /* would be nice to notify the world of a problem but where and how?? */	\
	if (!reg->open || reg->was_open)								\
		continue;										\
	if (!GTM_PROBE(SIZEOF(gd_segment), (reg)->dyn.addr, READ))					\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	if ((dba_bg != (reg)->dyn.addr->acc_meth) && (dba_mm != (reg)->dyn.addr->acc_meth))		\
		continue;										\
	if (!GTM_PROBE(SIZEOF(file_control), (reg)->dyn.addr->file_cntl, READ))				\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	if (!GTM_PROBE(SIZEOF(GDS_INFO), (reg)->dyn.addr->file_cntl->file_info, READ))			\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	csa = &(FILE_INFO((reg)))->s_addrs;								\
	if (!GTM_PROBE(SIZEOF(sgmnt_addrs), csa, WRITE))						\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	assert(reg->read_only && !csa->read_write || !reg->read_only && csa->read_write);

#ifdef DEBUG_CHECK_LATCH
#  define DEBUG_LATCH(x) x
#else
#  define DEBUG_LATCH(x)
#endif

#ifdef VMS
/* Use compswap_secshr instead of compswap in our expansions */
#  define compswap compswap_secshr
#  define SALVAGE_UNIX_LATCH(X, is_exiting)
#else
#  define SALVAGE_UNIX_LATCH_DBCRIT(X, is_exiting, wcblocked)								\
{	/* "wcblocked" is relevant only if X is the database crit semaphore. In this case, BEFORE salvaging crit,	\
	 * (but AFTER ensuring the previous holder pid is dead) we need to set cnl->wc_blocked to TRUE to		\
	 * ensure whoever grabs crit next does a cache-recovery. This is necessary in case previous holder of crit	\
	 * had set some cr->in_cw_set to a non-zero value. Not doing cache recovery could cause incorrect GTMASSERTs	\
	 * in PIN_CACHE_RECORD macro in t_end/tp_tend.									\
	 */														\
	uint4 pid;													\
															\
	if ((pid = (X)->u.parts.latch_pid) == rundown_process_id)							\
	{														\
		if (is_exiting)												\
		{													\
			SET_LATCH_GLOBAL(X, LOCK_AVAILABLE);								\
			DEBUG_LATCH(util_out_print("Latch cleaned up", FLUSH));						\
		}													\
	} else if (0 != pid && FALSE == is_proc_alive(pid, UNIX_ONLY(0) VMS_ONLY((X)->u.parts.latch_image_count)))	\
	{														\
		(wcblocked) = TRUE;											\
		DEBUG_LATCH(util_out_print("Orphaned latch cleaned up", TRUE));						\
		COMPSWAP_UNLOCK((X), pid, (X)->u.parts.latch_image_count, LOCK_AVAILABLE, 0);				\
	}														\
}

/* The SALVAGE_UNIX_LATCH macro needs to do exactly the same thing as done by the SALVAGE_UNIX_LATCH_DBCRIT	\
 * macro except that we dont need any special set of wc_blocked to TRUE. So we pass in a dummy variable		\
 * (instead of cnl->wc_blocked) to be set to TRUE in case the latch is salvaged.				\
 */														\
#define	SALVAGE_UNIX_LATCH(X, is_exiting)									\
{														\
	boolean_t	dummy;											\
														\
	SALVAGE_UNIX_LATCH_DBCRIT(X, is_exiting, dummy);							\
}

GBLREF	uint4		process_id;	/* Used in xxx_SWAPLOCK macros .. has same value as rundown_process_id on UNIX */
GBLREF	volatile int4	crit_count;
#endif

GBLDEF gd_addr_fn_ptr	get_next_gdr_addrs;
GBLDEF cw_set_element	*cw_set_addrs;
GBLDEF sgm_info		**first_sgm_info_addrs;
GBLDEF sgm_info		**first_tp_si_by_ftok_addrs;
GBLDEF unsigned char	*cw_depth_addrs;
GBLDEF uint4		rundown_process_id;
GBLDEF uint4		rundown_image_count;
GBLDEF int4		rundown_os_page_size;
GBLDEF gd_region	**jnlpool_reg_addrs;
GBLDEF inctn_opcode_t	*inctn_opcode_addrs;
GBLDEF inctn_detail_t	*inctn_detail_addrs;
GBLDEF uint4		*dollar_tlevel_addrs;
GBLDEF uint4		*update_trans_addrs;
GBLDEF sgmnt_addrs	**cs_addrs_addrs;
GBLDEF sgmnt_addrs 	**kip_csa_addrs;
GBLDEF boolean_t	*need_kip_incr_addrs;
GBLDEF trans_num	*start_tn_addrs;

#ifdef UNIX
GBLREF	short			crash_count;
GBLREF	node_local_ptr_t	locknl;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	boolean_t		dse_running;
GBLREF	boolean_t		certify_all_blocks;
GBLREF	gd_region		*gv_cur_region;		/* for the LOCK_HIST macro in the RELEASE_BUFF_UPDATE_LOCK macro */
GBLREF	node_local_ptr_t	locknl;			/* set explicitly before invoking RELEASE_BUFF_UPDATE_LOCK macro */
GBLREF	int4			strm_index;
GBLREF	jnl_gbls_t		jgbl;
#endif

#ifdef DEBUG
GBLREF	sgmnt_addrs		*cs_addrs;
#endif

error_def(ERR_WCBLOCKED);

typedef enum
{
	REG_COMMIT_UNSTARTED = 0,/* indicates that GT.M has not committed even one cse in this region */
	REG_COMMIT_PARTIAL,	 /* indicates that GT.M has committed at least one but not all cses for this region */
	REG_COMMIT_COMPLETE	 /* indicates that GT.M has already committed all cw-set-elements for this region */
} commit_type;

boolean_t	secshr_tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);

void secshr_db_clnup(enum secshr_db_state secshr_state)
{
	unsigned char		*chain_ptr;
	char			*wcblocked_ptr;
	uint4			dlr_tlevel;
	boolean_t		is_bg, jnlpool_reg, do_accounting, first_time = TRUE, is_exiting;
	boolean_t		kip_csa_usable, needkipincr;
	uint4			upd_trans; /* a copy of the global variable "update_trans" which is needed for VMS STOP/ID case */
	boolean_t		tp_update_underway = FALSE;	/* set to TRUE if TP commit was in progress or complete */
	boolean_t		non_tp_update_underway = FALSE;	/* set to TRUE if non-TP commit was in progress or complete */
	boolean_t		update_underway = FALSE;	/* set to TRUE if either TP or non-TP commit was underway */
	boolean_t		set_wc_blocked = FALSE;		/* set to TRUE if cnl->wc_blocked needs to be set */
	boolean_t		dont_reset_data_invalid;	/* set to TRUE in case cr->data_invalid was TRUE in phase2 */
	int			max_bts;
	unsigned int		lcnt;
	cache_rec_ptr_t		clru, cr, cr_alt, cr_top, start_cr, actual_cr;
	cache_que_heads_ptr_t	cache_state;
	cw_set_element		*cs, *cs_ptr, *cs_top, *first_cw_set, *nxt, *orig_cs;
	gd_addr			*gd_header;
	gd_region		*reg, *reg_top;
	jnl_buffer_ptr_t	jbp;
	off_chain		chain;
	sgm_info		*si, *firstsgminfo;
	sgmnt_addrs		*csa, *csaddrs;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		blk_ptr;
	blk_hdr_ptr_t		blk_hdr_ptr;
	jnlpool_ctl_ptr_t	jpl;
	jnldata_hdr_ptr_t	jh;
	uint4			cumul_jnl_rec_len, jsize, new_write, imgcnt;
	pid_t			pid;
	sm_uc_ptr_t		bufstart;
	int4			bufindx;	/* should be the same type as "csd->bt_buckets" */
	commit_type		this_reg_commit_type;	/* indicate the type of commit of a given region in a TP transaction */
	gv_namehead		*gvt = NULL, *gvtarget;
	srch_blk_status		*t1;
	trans_num		currtn;
	int4			n;
#	ifdef VMS
	uint4			process_id;	/* needed for the UNPIN_CACHE_RECORD macro */
#	endif
	GTM_SNAPSHOT_ONLY(
		snapshot_context_ptr_t	lcl_ss_ctx;
		cache_rec_ptr_t		snapshot_cr;
	)
#	ifdef UNIX
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	if (NULL == get_next_gdr_addrs)
		return;
	/*
	 * secshr_db_clnup can be called with one of the following three values for "secshr_state"
	 *
	 * 	a) NORMAL_TERMINATION   --> We are called from the exit-handler for precautionary cleanup.
	 * 				    We should NEVER be in the midst of a database update in this case.
	 * 	b) COMMIT_INCOMPLETE    --> We are called from t_commit_cleanup.
	 * 				    We should ALWAYS be in the midst of a database update in this case.
	 * 	c) ABNORMAL_TERMINATION --> This is currently VMS ONLY. This process received a STOP/ID.
	 * 				    We can POSSIBLY be in the midst of a database update in this case.
	 * 				    When UNIX boxes allow kernel extensions, this can be made to handle "kill -9" too.
	 *
	 * If we are in the midst of a database update, then depending on the stage of the commit we are in,
	 * 	we need to ROLL-BACK (undo the partial commit) or ROLL-FORWARD (complete the partial commit) the database update.
	 *
	 * t_commit_cleanup handles the ROLL-BACK and secshr_db_clnup handles the ROLL-FORWARD
	 *
	 * For all error conditions in the database commit logic, t_commit_cleanup gets control first.
	 * If then determines whether to do a ROLL-BACK or a ROLL-FORWARD.
	 * If a ROLL-BACK needs to be done, then t_commit_cleanup handles it all by itself and we will not come here.
	 * If a ROLL-FORWARD needs to be done, then t_commit_cleanup invokes secshr_db_clnup.
	 * 	In this case, secshr_db_clnup will be called with a "secshr_state" value of "COMMIT_INCOMPLETE".
	 *
	 * In case of a STOP/ID in VMS, secshr_db_clnup is directly invoked with a "secshr_state" value of "ABNORMAL_TERMINATION".
	 * Irrespective of whether we are in the midst of a database commit or not, t_commit_cleanup does not get control.
	 * Since the process can POSSIBLY be in the midst of a database update while it was STOP/IDed,
	 * 	the logic for determining whether it is a ROLL-BACK or a ROLL-FORWARD needs to also be in secshr_db_clnup.
	 * If it is determined that a ROLL-FORWARD needs to be done, secshr_db_clnup takes care of it by itself.
	 * But if a ROLL-BACK needs to be done, then secshr_db_clnup DOES NOT invoke t_commit_cleanup.
	 * Instead it sets cnl->wc_blocked to TRUE thereby ensuring the next process that gets CRIT does a cache recovery
	 * 	which will take care of doing more than the ROLL-BACK that t_commit_cleanup would have otherwise done.
	 *
	 * The logic for determining if it is a ROLL-BACK or ROLL-FORWARD is explained below.
	 * The commit logic flow in tp_tend and t_end can be captured as follows. Note that in t_end there is only one region.
	 *
	 *  1) Get crit on all regions
	 *  2) Get crit on jnlpool
	 *  3) jnlpool_ctl->early_write_addr += delta;
	 *       For each participating region being UPDATED
	 *       {
	 *  4)     csd->trans_hist.early_tn++;
	 *         Write journal records
	 *  5)     csa->hdr->reg_seqno = jnlpool_ctl->jnl_seqno + 1;
	 *       }
	 *       For each participating region being UPDATED
	 *       {
	 *  6)	    csa->t_commit_crit = T_COMMIT_CRIT_PHASE1;
	 *             For every cw-set-element of this region
	 *             {
	 *  6a)          Commit this particular block PHASE1 (inside crit).
	 *             }
	 *  7)       csa->t_commit_crit = T_COMMIT_CRIT_PHASE2;
	 *  8)     csd->trans_hist.curr_tn++;
	 *       }
	 *  9) jnlpool_ctl->write_addr = jnlpool_ctl->early_write_addr;
	 * 10) jnlpool_ctl->jnl_seqno++;
	 * 11) Release crit on all db regions
	 * 12) Release crit on jnlpool
	 *       For each participating region being UPDATED
	 *       {
	 *             For every cw-set-element of this region
	 *             {
	 * 13)           Commit this particular block PHASE2 (outside crit).
	 * 14)           cs->mode = gds_t_committed;
	 *             }
	 * 15)       csa->t_commit_crit = FALSE;
	 *       }
	 *
	 * If a TP transaction has proceeded to step (6) for at least one region, then "tp_update_underway" is set to TRUE
	 * and the transaction cannot be rolled back but has to be committed. Otherwise the transaction is rolled back.
	 *
	 * If a non-TP transaction has proceeded to step (6), then "non_tp_update_underway" is set to TRUE
	 * and the transaction cannot be rolled back but has to be committed. Otherwise the transaction is rolled back.
	 */
	UNIX_ONLY(assert(rundown_process_id == process_id);)
	VMS_ONLY(assert(rundown_process_id);)
	VMS_ONLY(process_id = rundown_process_id;)	/* used by the UNPIN_CACHE_RECORD macro */
	is_exiting = (ABNORMAL_TERMINATION == secshr_state) || (NORMAL_TERMINATION == secshr_state);
	if (GTM_PROBE(SIZEOF(*dollar_tlevel_addrs), dollar_tlevel_addrs, READ))
		dlr_tlevel = *dollar_tlevel_addrs;
	else
	{
		assert(FALSE);
		dlr_tlevel = FALSE;
	}
	if (dlr_tlevel && GTM_PROBE(SIZEOF(*first_tp_si_by_ftok_addrs), first_tp_si_by_ftok_addrs, READ))
	{	/* Determine update_underway for TP transaction. A similar check is done in t_commit_cleanup as well.
		 * Regions are committed in the ftok order using "first_tp_si_by_ftok". Also crit is released on each region
		 * as the commit completes. Take that into account while determining if update is underway.
		 */
		for (si = *first_tp_si_by_ftok_addrs; NULL != si; si = si->next_tp_si_by_ftok)
		{
			if (GTM_PROBE(SIZEOF(sgm_info), si, READ))
			{
				assert(GTM_PROBE(SIZEOF(cw_set_element), si->first_cw_set, READ) || (NULL == si->first_cw_set));
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
				if (GTM_PROBE(SIZEOF(cw_set_element), si->first_cw_set, READ))
				{	/* Note that SECSHR_PROBE_REGION does a "continue" if any probes fail. */
					csa = si->tp_csa;
					if (!GTM_PROBE(SIZEOF(sgmnt_addrs), csa, READ))
						continue;
					if (T_UPDATE_UNDERWAY(csa))
					{
						tp_update_underway = TRUE;
						update_underway = TRUE;
						break;
					}
				}
			} else
			{
				assert(FALSE);
				break;
			}
		}
	}
	if (!dlr_tlevel)
	{	/* determine update_underway for non-TP transaction */
		upd_trans = FALSE;
		if (GTM_PROBE(SIZEOF(*update_trans_addrs), update_trans_addrs, READ))
			upd_trans = *update_trans_addrs;
		csaddrs = NULL;
		if (GTM_PROBE(SIZEOF(*cs_addrs_addrs), cs_addrs_addrs, READ))
			csaddrs = *cs_addrs_addrs;
		if (GTM_PROBE(SIZEOF(sgmnt_addrs), csaddrs, READ))
		{
			if (csaddrs->now_crit && (UPDTRNS_TCOMMIT_STARTED_MASK & upd_trans) || T_UPDATE_UNDERWAY(csaddrs))
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
	for (gd_header = (*get_next_gdr_addrs)(NULL);  NULL != gd_header;  gd_header = (*get_next_gdr_addrs)(gd_header))
	{
		if (!GTM_PROBE(SIZEOF(gd_addr), gd_header, READ))
			break;	/* if gd_header is accessible */
		for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions;  reg < reg_top;  reg++)
		{
			SECSHR_PROBE_REGION(reg);	/* SECSHR_PROBE_REGION sets csa */
			csd = csa->hdr;
			if (!GTM_PROBE(SIZEOF(sgmnt_data), csd, WRITE))
			{
				assert(FALSE);
				continue; /* would be nice to notify the world of a problem but where and how? */
			}
			cnl = csa->nl;
			if (!GTM_PROBE(NODE_LOCAL_SIZE_DBS, cnl, WRITE))
			{
				assert(FALSE);
				continue; /* would be nice to notify the world of a problem but where and how? */
			}
			is_bg = (csd->acc_meth == dba_bg);
			do_accounting = FALSE;	/* used by SECSHR_ACCOUNTING macro */
			/* do SECSHR_ACCOUNTING only if holding crit (to avoid another process' normal termination call
			 * to secshr_db_clnup from overwriting whatever important information we wrote. if we are in
			 * crit, for the next process to overwrite us it needs to get crit which in turn will invoke
			 * wcs_recover which in turn will send whatever we wrote to the operator log).
			 * also cannot update csd if MM and read-only. take care of that too. */
			if (csa->now_crit && (csa->read_write || is_bg))
			{	/* start accounting */
				cnl->secshr_ops_index = 0;
				do_accounting = TRUE;	/* used by SECSHR_ACCOUNTING macro */
			}
			SECSHR_ACCOUNTING(4);	/* 4 is the number of arguments following including self */
			SECSHR_ACCOUNTING(__LINE__);
			SECSHR_ACCOUNTING(rundown_process_id);
			SECSHR_ACCOUNTING(secshr_state);
			if (csa->ti != &csd->trans_hist)
			{
				SECSHR_ACCOUNTING(4);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)csa->ti);
				SECSHR_ACCOUNTING((INTPTR_T)&csd->trans_hist);
				csa->ti = &csd->trans_hist;	/* better to correct and proceed than to stop */
			}
			SECSHR_ACCOUNTING(3);	/* 3 is the number of arguments following including self */
			SECSHR_ACCOUNTING(__LINE__);
			SECSHR_ACCOUNTING(csd->trans_hist.curr_tn);
			if (is_exiting)
			{	/* If we hold any latches in the node_local area, release them. Note we do not check
				   db_latch here because it is never used by the compare and swap logic but rather
				   the aswp logic. Since it is only used for the 3 state cache record lock and
				   separate recovery exists for it, we do not do anything with it here.
				*/
				SALVAGE_UNIX_LATCH(&cnl->wc_var_lock, is_exiting);
				if (ABNORMAL_TERMINATION == secshr_state)
				{
					if (csa->timer)
					{
						if (-1 < cnl->wcs_timers) /* private flag is optimistic: dont overdo */
							CAREFUL_DECR_CNT(&cnl->wcs_timers, &cnl->wc_var_lock);
						csa->timer = FALSE;
					}
					if (csa->read_write && csa->ref_cnt)
					{
						assert(0 < cnl->ref_cnt);
						csa->ref_cnt--;
						assert(!csa->ref_cnt);
						CAREFUL_DECR_CNT(&cnl->ref_cnt, &cnl->wc_var_lock);
					}
				}
				if ((csa->in_wtstart) && (0 < cnl->in_wtstart))
				{
					CAREFUL_DECR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);
					assert(0 < cnl->intent_wtstart);
					if (0 < cnl->intent_wtstart)
						CAREFUL_DECR_CNT(&cnl->intent_wtstart, &cnl->wc_var_lock);
				}
				csa->in_wtstart = FALSE;	/* Let wcs_wtstart run for exit processing */
				if (cnl->wcsflu_pid == rundown_process_id)
					cnl->wcsflu_pid = 0;
			}
			set_wc_blocked = FALSE;
			if (is_bg)
			{
				if ((0 == cnl->sec_size) || !GTM_PROBE(cnl->sec_size VMS_ONLY(* OS_PAGELET_SIZE), cnl, WRITE))
				{
					SECSHR_ACCOUNTING(3);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(cnl->sec_size VMS_ONLY(* OS_PAGELET_SIZE));
					assert(FALSE);
					continue;
				}
				cache_state = csa->acc_meth.bg.cache_state;
				if (!GTM_PROBE(SIZEOF(cache_que_heads), cache_state, WRITE))
				{
					SECSHR_ACCOUNTING(3);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING((INTPTR_T)cache_state);
					assert(FALSE);
					continue;
				}
				SALVAGE_UNIX_LATCH(&cache_state->cacheq_active.latch, is_exiting);
				start_cr = cache_state->cache_array + csd->bt_buckets;
				max_bts = csd->n_bts;
				if (!GTM_PROBE((uint4)(max_bts * SIZEOF(cache_rec)), start_cr, WRITE))
				{
					SECSHR_ACCOUNTING(3);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING((INTPTR_T)start_cr);
					assert(FALSE);
					continue;
				}
				cr_top = start_cr + max_bts;
				if (is_exiting)
				{
					for (cr = start_cr;  cr < cr_top;  cr++)
					{	/* walk the cache looking for incomplete writes and reads issued by self */
						VMS_ONLY(
							if ((0 == cr->iosb.cond) && (cr->epid == rundown_process_id))
						        {
								cr->shmpool_blk_off = 0;	/* Cut link to reformat blk */
								cr->wip_stopped = TRUE;
							}
						)
						SALVAGE_UNIX_LATCH(&cr->rip_latch, is_exiting);
						if ((cr->r_epid == rundown_process_id) && (0 == cr->dirty) && (0 == cr->in_cw_set))
						{	/* increment cycle for blk number changes (for tp_hist) */
							cr->cycle++;
							cr->blk = CR_BLKEMPTY;
							/* ensure no bt points to this cr for empty blk */
							assert(0 == cr->bt_index);
							/* don't mess with ownership the I/O may not yet be cancelled;
							 * ownership will be cleared by whoever gets stuck waiting
							 * for the buffer */
						}
					}
				}
			}
			first_cw_set = cs = NULL;
			/* If tp_update_underway has been determined to be TRUE, then we are guaranteed we have a well formed
			 * ftok ordered linked list ("first_tp_si_by_ftok") so we can safely use this.
			 */
			if (tp_update_underway)
			{	/* this is constructed to deal with the issue of reg != si->gv_cur_region
				 * due to the possibility of multiple global directories pointing to regions
				 * that resolve to the same physical file; was_open prevents processing the segment
				 * more than once, so this code matches on the file rather than the region to make sure
				 * that it gets processed at least once */
				for (si = *first_tp_si_by_ftok_addrs; NULL != si; si = si->next_tp_si_by_ftok)
				{
					if (!GTM_PROBE(SIZEOF(sgm_info), si, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING((INTPTR_T)si);
						assert(FALSE);
						break;
					} else if (!GTM_PROBE(SIZEOF(gd_region), si->gv_cur_region, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING((INTPTR_T)si->gv_cur_region);
						assert(FALSE);
						continue;
					} else if (!GTM_PROBE(SIZEOF(gd_segment), si->gv_cur_region->dyn.addr, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING((INTPTR_T)si->gv_cur_region->dyn.addr);
						assert(FALSE);
						continue;
					} else if (si->gv_cur_region->dyn.addr->file_cntl == reg->dyn.addr->file_cntl)
					{
						cs = si->first_cw_set;
						if (cs && GTM_PROBE(SIZEOF(cw_set_element), cs, READ))
						{
							while (cs->high_tlevel)
							{
								if (GTM_PROBE(SIZEOF(cw_set_element),
											cs->high_tlevel, READ))
									cs = cs->high_tlevel;
								else
								{
									SECSHR_ACCOUNTING(3);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING((INTPTR_T)cs->high_tlevel);
									assert(FALSE);
									first_cw_set = cs = NULL;
									break;
								}
							}
						}
						first_cw_set = cs;
						break;
					}
				}
			} else if (!dlr_tlevel && csa->t_commit_crit)
			{
				if (!GTM_PROBE(SIZEOF(unsigned char), cw_depth_addrs, READ))
				{
					SECSHR_ACCOUNTING(3);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING((INTPTR_T)cw_depth_addrs);
					assert(FALSE);
				} else
				{	/* csa->t_commit_crit being TRUE is a clear cut indication that we have
					 * reached stage (6). ROLL-FORWARD the commit unconditionally.
					 */
					if (0 != *cw_depth_addrs)
					{
						first_cw_set = cs = cw_set_addrs;
						cs_top = cs + *cw_depth_addrs;
					}
					/* else is the case where we had a duplicate set that did not update any cw-set */
					assert(!tp_update_underway);
					assert(non_tp_update_underway);	/* should have already determined update is underway */
					if (!non_tp_update_underway)
					{	/* This is a situation where we are in non-TP and have a region that we hold
						 * crit in and are in the midst of commit but this region was not the current
						 * region when we entered secshr_db_clnup. This is an out-of-design situation
						 * that we want to catch in Unix (not VMS because it runs in kernel mode).
						 */
						UNIX_ONLY(GTMASSERT;)	/* in Unix we want to catch this situation even in pro */
					}
					non_tp_update_underway = TRUE;	/* just in case */
					update_underway = TRUE;		/* just in case */
				}
			}
			assert(!tp_update_underway || (NULL == first_cw_set) || (NULL != si));
			/* It is possible that we were in the midst of a non-TP commit for this region at or past stage (7)
			 * but first_cw_set is NULL. This is a case of duplicate SET with zero cw_set_depth. In this case,
			 * dont have any cw-set-elements to commit. The only thing remaining to do is steps (9) through (12)
			 * which are done later in this function.
			 */
			assert((FALSE == csa->t_commit_crit) || (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit)
								|| (T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit));
			assert(!csa->t_commit_crit || (NULL != first_cw_set));	/* dont miss out committing a region */
			/* Skip processing region in case of a multi-region TP transaction where this region is already committed */
			assert((NULL == first_cw_set) || csa->now_crit || csa->t_commit_crit || tp_update_underway);
			if ((csa->now_crit || csa->t_commit_crit) && (NULL != first_cw_set))
			{
				SECSHR_ACCOUNTING(6);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING(csa->now_crit);
				SECSHR_ACCOUNTING(csa->t_commit_crit);
				SECSHR_ACCOUNTING(csd->trans_hist.early_tn);
				SECSHR_ACCOUNTING(csd->trans_hist.curr_tn);
				assert(non_tp_update_underway || tp_update_underway);
				assert(!non_tp_update_underway || !tp_update_underway);
				if (is_bg)
				{
					clru = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cnl->cur_lru_cache_rec_off);
					lcnt = 0;
				}
				assert((T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit) || csa->now_crit);
				if (T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit)
				{	/* in PHASE1 so hold crit AND have noted down valid value in csa->prev_free_blks */
					assert(NORMAL_TERMINATION != secshr_state); /* for normal termination we should not
										     * have been in the midst of commit */
					assert(csa->now_crit);
					csd->trans_hist.free_blocks = csa->prev_free_blks;
				}
				SECSHR_ACCOUNTING(tp_update_underway ? 6 : 7);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)first_cw_set);
				SECSHR_ACCOUNTING(tp_update_underway);
				SECSHR_ACCOUNTING(non_tp_update_underway);
				if (!tp_update_underway)
				{
					SECSHR_ACCOUNTING((INTPTR_T)cs_top);
					SECSHR_ACCOUNTING(*cw_depth_addrs);
				} else
				{
					SECSHR_ACCOUNTING(si->cw_set_depth);
					this_reg_commit_type = REG_COMMIT_UNSTARTED; /* assume GT.M did no commits in this region */
					/* Note that "this_reg_commit_type" is uninitialized if "tp_update_underway" is not TRUE
					 * so should always be used within an "if (tp_update_underway)" */
				}
				/* Determine transaction number to use for the gvcst_*_build functions.
				 * If not phase2, then we have crit, so it is the same as the current database transaction number.
				 * If phase2, then we dont have crit, so use value stored in "start_tn" or "si->start_tn".
				 */
				if (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit)
					currtn = csd->trans_hist.curr_tn;
				else
				{
					if (!tp_update_underway)
					{
						if (GTM_PROBE(SIZEOF(*start_tn_addrs), start_tn_addrs, READ))
							currtn = *start_tn_addrs;
						else
						{
							assert(FALSE);
							/* dont know how this is possible, but in this case use curr db tn - 1 */
							currtn = csd->trans_hist.curr_tn - 1;
						}
					} else
						currtn = si->start_tn;
					assert(currtn < csd->trans_hist.curr_tn);
				}
				for (; (tp_update_underway  &&  NULL != cs) || (!tp_update_underway  &&  cs < cs_top);
					cs = tp_update_underway ? orig_cs->next_cw_set : (cs + 1))
				{
					dont_reset_data_invalid = FALSE;
					if (tp_update_underway)
					{
						orig_cs = cs;
						if (cs && GTM_PROBE(SIZEOF(cw_set_element), cs, READ))
						{
							while (cs->high_tlevel)
							{
								if (GTM_PROBE(SIZEOF(cw_set_element),
											cs->high_tlevel, READ))
									cs = cs->high_tlevel;
								else
								{
									SECSHR_ACCOUNTING(3);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING((INTPTR_T)cs->high_tlevel);
									assert(FALSE);
									cs = NULL;
									break;
								}
							}
						}
					}
					if (!GTM_PROBE(SIZEOF(cw_set_element), cs, WRITE))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING((INTPTR_T)cs);
						assert(FALSE);
						break;
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
							 * that is going to unpin all the cache-records so we dont take the
							 * trouble to do it here.
							 */
						} else
						{	/* Currently there are only two possibilities and both are in TP.
							 * In either case, need to simulate what tp_tend would have done which
							 * is to build a private copy right now if this is the first phase of
							 * commit (i.e. we hold crit) as this could be needed in the 2nd phase
							 * of KILL.
							 */
							assert(tp_update_underway);
							assert((kill_t_write == cs->mode) || (kill_t_create == cs->mode));
							if (csa->now_crit && (!cs->done))
							{
#								ifdef UNIX
								/* Initialize cs->new_buff to non-NULL since sec_shr_blk_build
								 * expects this. For VMS, tp_tend would have done this already.
								 */
								if (NULL == cs->new_buff)
									cs->new_buff = (unsigned char *)
											get_new_free_element(si->new_buff_list);
#								endif
								assert(NULL != cs->new_buff);
								blk_ptr = (sm_uc_ptr_t)cs->new_buff;
								/* No need to probe blk_ptr as sec_shr_blk_build does that */
								if (FALSE == sec_shr_blk_build(csa, csd, is_bg, cs, blk_ptr,
												currtn))
								{
									SECSHR_ACCOUNTING(10);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING((INTPTR_T)cs);
									SECSHR_ACCOUNTING(cs->blk);
									SECSHR_ACCOUNTING(cs->level);
									SECSHR_ACCOUNTING(cs->done);
									SECSHR_ACCOUNTING(cs->forward_process);
									SECSHR_ACCOUNTING(cs->first_copy);
									SECSHR_ACCOUNTING((INTPTR_T)cs->upd_addr);
									SECSHR_ACCOUNTING((INTPTR_T)cs->new_buff);
									assert(FALSE);
									continue;
								} else if (cs->ins_off != 0)
								{
									if ((cs->ins_off
										> ((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
										|| (cs->ins_off
										 < (SIZEOF(blk_hdr) + SIZEOF(rec_hdr))))
									{
										SECSHR_ACCOUNTING(7);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING((INTPTR_T)cs);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(cs->index);
										SECSHR_ACCOUNTING(cs->ins_off);
										SECSHR_ACCOUNTING(
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
									if (!(GTM_PROBE(SIZEOF(int4), chain_ptr, WRITE)))
									{
										SECSHR_ACCOUNTING(5);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING((INTPTR_T)cs);
										SECSHR_ACCOUNTING(cs->ins_off);
										SECSHR_ACCOUNTING((INTPTR_T)chain_ptr);
										assert(FALSE);
										continue;
									}
									GET_LONGP(chain_ptr, &chain);
									cs->ins_off = cs->next_off = 0;
								}
								cs->done = TRUE;
								assert(NULL != cs->blk_target);
								/* cert_blk cannot be done in VMS as it is a heavyweight routine
								 * and cannot be pulled into GTMSECSHR. Hence do it only in Unix.
								 */
								UNIX_ONLY(assert(NULL == gvt);)
								UNIX_ONLY(CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region,
											cs, cs->new_buff, gvt);)
							}
						}
						cs->old_mode = (int4)cs->mode;
						assert(0 < cs->old_mode);
						cs->mode = gds_t_committed;
						continue;
					}
					if (gds_t_committed == cs->mode)
					{	/* already processed */
						assert(0 < cs->old_mode);
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
						assert(!dlr_tlevel || (gds_t_write_root != cs->old_mode));
						assert(gds_t_committed != cs->old_mode);
						if (gds_t_committed > cs->old_mode)
						{
							if (!GTM_PROBE(SIZEOF(cache_rec), cr, WRITE))
							{
								SECSHR_ACCOUNTING(4);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING((INTPTR_T)cs);
								SECSHR_ACCOUNTING((INTPTR_T)cr);
								assert(FALSE);
							} else if (rundown_process_id == cr->in_tend)
							{	/* Not sure how this is possible */
								assert(FALSE);
							}
						} else
						{	/* For the kill_t_* case, cs->cr will be NULL as bg_update was not invoked
							 * and the cw-set-elements were memset to 0 in TP. But for gds_t_write_root
							 * and gds_t_busy2free, they are non-TP ONLY modes and cses are not
							 * initialized so cant check for NULL cr. Thankfully "n_gds_t_op" demarcates
							 * the boundaries between non-TP only and TP only modes. So use that.
							 */
							assert((n_gds_t_op > cs->old_mode) || (NULL == cr));
						}
						continue;
					}
					/* Since we are going to build blocks at this point, unconditionally set wc_blocked
					 * (after finishing commits) to trigger wcs_recover even though we might not be
					 * holding crit at this point.
					 */
					set_wc_blocked = TRUE;
					assert(NORMAL_TERMINATION != secshr_state); /* for normal termination we should not
										     * have been in the midst of commit */
					if (tp_update_underway)
					{	/* Since the current cse has not been committed, this is a partial
						 * GT.M commit in this region even if we have already seen committed cses.
						 */
						this_reg_commit_type = REG_COMMIT_PARTIAL;
					}
					if (is_bg)
					{
						if (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit)
						{	/* We are not yet in phase2 which means we hold crit on this region,
							 * so have to find out a free cache-record we can dump our updates onto.
							 */
							for ( ; lcnt++ < max_bts; )
							{	/* find any available cr */
								if (++clru >= cr_top)
									clru = start_cr;
								assert(!clru->stopped);
								if (!clru->stopped && (0 == clru->dirty)
									&& (0 == clru->in_cw_set)
									&& (!clru->in_tend)
									&& (-1 == clru->read_in_progress)
									&& GTM_PROBE(csd->blk_size,
										GDS_ANY_REL2ABS(csa, clru->buffaddr), WRITE))
									break;
							}
							if (lcnt >= max_bts)
							{
								SECSHR_ACCOUNTING(9);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING((INTPTR_T)cs);
								SECSHR_ACCOUNTING(cs->blk);
								SECSHR_ACCOUNTING(cs->tn);
								SECSHR_ACCOUNTING(cs->level);
								SECSHR_ACCOUNTING(cs->done);
								SECSHR_ACCOUNTING(cs->forward_process);
								SECSHR_ACCOUNTING(cs->first_copy);
								assert(FALSE);
								continue;
							}
							cr = clru;
							cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
							assert(cs->blk < csd->trans_hist.total_blks);
							cr->blk = cs->blk;
							assert(CR_BLKEMPTY != cr->blk);
							cr->jnl_addr = cs->jnl_freeaddr;
							cr->stopped = TRUE;
							/* Keep cs->cr and t1->cr uptodate to ensure clue will be accurate */
							cs->cr = cr;
							cs->cycle = cr->cycle;
							if (!IS_BITMAP_BLK(cs->blk))
							{	/* Not a bitmap block, update clue history to reflect new cr */
								assert((0 <= cs->level) && (MAX_BT_DEPTH > cs->level));
								gvtarget = cs->blk_target;
								assert((MAX_BT_DEPTH + 1)
									== (SIZEOF(gvtarget->hist.h)
										/ SIZEOF(gvtarget->hist.h[0])));
								if ((0 <= cs->level) && (MAX_BT_DEPTH > cs->level)
									&& GTM_PROBE(SIZEOF(gv_namehead), gvtarget, WRITE)
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
						{	/* We are in PHASE2 of the commit (i.e. have completed PHASE1 for ALL cses)
							 * We have already picked out a cr for the commit. Use that.
							 */
							cr = cs->cr;
							if (!GTM_PROBE(SIZEOF(cache_rec), cr, WRITE))
							{
								SECSHR_ACCOUNTING(4);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING((INTPTR_T)cs);
								SECSHR_ACCOUNTING((INTPTR_T)cr);
								assert(FALSE);
								continue;
							}
							if (rundown_process_id != cr->in_tend)
							{	/* phase2 commit is already complete for this cse but we got
								 * interrupted before setting cs->mode to gds_t_committed.
								 * Possible that this cache-record is not placed in the active
								 * queue properly. Any case set_wc_blocked is already set so that
								 * should take care of invoking wcs_recover to fix the queues.
								 */
								assert(rundown_process_id != cr->in_cw_set);
								assert(rundown_process_id != cr->data_invalid);
								continue;
							}
							assert(rundown_process_id == cr->in_cw_set);
							assert(cr->blk == cs->cr->blk);
							if (cr->data_invalid)
							{	/* Buffer is already in middle of update. Since blk builds are
								 * not redoable, db is in danger whether or not we redo the build.
								 * Since, skipping the build is guaranteed to give us integrity
								 * errors, we redo the build hoping it will have at least a 50%
								 * chance of resulting in a clean block. Make sure data_invalid
								 * flag is set until the next cache-recovery (wcs_recover will
								 * send a DBDANGER syslog message for this block to alert of
								 * potential database damage) by setting dont_reset_data_invalid.
								 */
								SECSHR_ACCOUNTING(6);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING((INTPTR_T)cs);
								SECSHR_ACCOUNTING((INTPTR_T)cr);
								SECSHR_ACCOUNTING(cr->blk);
								SECSHR_ACCOUNTING(cr->data_invalid);
								assert(FALSE);
								dont_reset_data_invalid = TRUE;
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
							DEBUG_ONLY(GTM_SNAPSHOT_ONLY(snapshot_cr = NULL;)) /* Will be set below */
							if (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit)
							{	/* Set "cr->twin" to point to "cs->old_block". This is not normal
								 * usage since "twin" usually points to a cache-record. But this
								 * is a special case where we want to record the before-image
								 * somewhere for wcs_recover to see and we are not allowed division
								 * operations in secshr_db_clnup (which is required to find out the
								 * corresponding cache-record). Hence we store the relative offset
								 * of "cs->old_block". This is a special case where "cr->twin" can
								 * be non-zero even in Unix. wcs_recover will recognize this special
								 * usage of "twin" (since cr->stopped is non-zero as well) and fix
								 * it. Note that in VMS, it is possible to have two other crs for
								 * the same block cr1, cr2 which are each twinned so we could end
								 * up with the following twin configuration.
								 *	cr1 <---> cr2 <--- cr
								 * Note cr->twin = cr2 is a one way link and stores "cs->old_block",
								 * while "cr1->twin" and "cr2->twin" store each other's cacherecord
								 * pointers.
								 */
#								ifdef UNIX
								bufstart = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, start_cr->buffaddr);
								bufindx = (int4)(cs->old_block - bufstart) / csd->blk_size;
								assert(0 <= bufindx);
								assert(bufindx < csd->n_bts);
								cr_alt = &start_cr[bufindx];
								assert(cr_alt != cr);
								assert(cs->blk == cr_alt->blk);
								assert(rundown_process_id == cr_alt->in_cw_set);
								snapshot_cr = cr_alt;
#								endif
								/* wcs_recover need not copy before images of FREE blocks
								 * to the backup buffer */
								if (!WAS_FREE(cs->blk_prior_state))
									cr->twin = GDS_ANY_ABS2REL(csa, cs->old_block);
							} else
							{	/* We have to finish phase2 update.
								 * If Unix, we backup the block right here instead of waiting for
								 * wcs_recover to do it. If VMS, we dont need to do anything as
								 * the block has already been backed up in phase1. See end of
								 * bg_update_phase1 for comment on why.
								 */
#								ifdef UNIX
								/* The following check is similar to the one in BG_BACKUP_BLOCK
								 * and the one in wcs_recover (where backup_block is invoked)
								 */
								blk_hdr_ptr = (blk_hdr_ptr_t)cs->old_block;
								assert(GDS_ANY_REL2ABS(csa, cr->buffaddr)
										== (sm_uc_ptr_t)blk_hdr_ptr);
								if (!WAS_FREE(cs->blk_prior_state) && (cr->blk >= cnl->nbb)
									&& (0 == csa->shmpool_buffer->failed)
									&& (blk_hdr_ptr->tn < csa->shmpool_buffer->backup_tn)
									&& (blk_hdr_ptr->tn >= csa->shmpool_buffer->inc_backup_tn))
								{
									backup_block(csa, cr->blk, cr, NULL);
									/* No need for us to flush the backup buffer.
									 * MUPIP BACKUP will anyways flush it at the end.
									 */
								}
								snapshot_cr = cr;
#								endif
							}
#							ifdef GTM_SNAPSHOT
							if (SNAPSHOTS_IN_PROG(csa))
							{
								lcl_ss_ctx = SS_CTX_CAST(csa->ss_ctx);
								assert(NULL != snapshot_cr);
								assert((snapshot_cr == cr) || (snapshot_cr == cr_alt));
								WRITE_SNAPSHOT_BLOCK(csa, snapshot_cr, NULL, snapshot_cr->blk,
											lcl_ss_ctx);
							}
#							endif
						}
						if (T_COMMIT_CRIT_PHASE2 != csa->t_commit_crit)
						{	/* Adjust blks_to_upgrd counter if not already done in phase1. The value of
							 * cs->old_mode if negative implies phase1 is complete on this cse so we
							 * dont need to do this adjustment again. If not we do the adjustment.
							 */
							assert((0 <= cs->old_mode) || (cs->old_mode == -cs->mode));
							if (0 <= cs->old_mode)
							{	/* the following code is very similar to that in bg_update */
								if (gds_t_acquired == cs->mode)
								{
									if (GDSV4 == csd->desired_db_format)
									{
										INCR_BLKS_TO_UPGRD(csa, csd, 1);
									}
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
									 * value in the local variable csaddrs.
									 */
									csaddrs = cs_addrs;	/* save cs_addrs in local */
									cs_addrs = csa;		/* set cs_addrs for db_csh_get */
									actual_cr = db_csh_get(cs->blk);
									cs_addrs = csaddrs;	/* restore cs_addrs */
									/* actual_cr can be NULL if the block is NOT in the cache.
									 * It can be CR_NOTVALID if the cache record originally
									 * containing this block got reused for a different block
									 * (i.e. cr->stopped = 1) as part of secshr_db_clnup.
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
						blk_ptr = MM_BASE_ADDR(csa) + (off_t)csd->blk_size * cs->blk;
						if (!GTM_PROBE(csd->blk_size, blk_ptr, WRITE))
						{
							SECSHR_ACCOUNTING(7);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING((INTPTR_T)cs);
							SECSHR_ACCOUNTING(cs->blk);
							SECSHR_ACCOUNTING((INTPTR_T)blk_ptr);
							SECSHR_ACCOUNTING(csd->blk_size);
							SECSHR_ACCOUNTING((INTPTR_T)(MM_BASE_ADDR(csa)));
							assert(FALSE);
							continue;
						}
					}
					/* The following block of code rolls forward steps (6a) and/or (13) of the commit */
					if (cs->mode == gds_t_writemap)
					{
						if (!GTM_PROBE(csd->blk_size, cs->old_block, READ))
						{
							SECSHR_ACCOUNTING(11);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING((INTPTR_T)cs);
							SECSHR_ACCOUNTING(cs->blk);
							SECSHR_ACCOUNTING(cs->tn);
							SECSHR_ACCOUNTING(cs->level);
							SECSHR_ACCOUNTING(cs->done);
							SECSHR_ACCOUNTING(cs->forward_process);
							SECSHR_ACCOUNTING(cs->first_copy);
							SECSHR_ACCOUNTING((INTPTR_T)cs->old_block);
							SECSHR_ACCOUNTING(csd->blk_size);
							assert(FALSE);
							continue;
						}
						memmove(blk_ptr, cs->old_block, csd->blk_size);
						if (FALSE == sec_shr_map_build(csa, (uint4*)cs->upd_addr, blk_ptr, cs,
							currtn, BM_SIZE(csd->bplmap)))
						{
							SECSHR_ACCOUNTING(11);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING((INTPTR_T)cs);
							SECSHR_ACCOUNTING(cs->blk);
							SECSHR_ACCOUNTING(cs->tn);
							SECSHR_ACCOUNTING(cs->level);
							SECSHR_ACCOUNTING(cs->done);
							SECSHR_ACCOUNTING(cs->forward_process);
							SECSHR_ACCOUNTING(cs->first_copy);
							SECSHR_ACCOUNTING((INTPTR_T)cs->upd_addr);
							SECSHR_ACCOUNTING((INTPTR_T)blk_ptr);
							assert(FALSE);
							continue;
						}
					} else
					{
						if (!tp_update_underway)
						{
							if (FALSE == sec_shr_blk_build(csa, csd, is_bg, cs, blk_ptr, currtn))
							{
								SECSHR_ACCOUNTING(10);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING((INTPTR_T)cs);
								SECSHR_ACCOUNTING(cs->blk);
								SECSHR_ACCOUNTING(cs->level);
								SECSHR_ACCOUNTING(cs->done);
								SECSHR_ACCOUNTING(cs->forward_process);
								SECSHR_ACCOUNTING(cs->first_copy);
								SECSHR_ACCOUNTING((INTPTR_T)cs->upd_addr);
								SECSHR_ACCOUNTING((INTPTR_T)blk_ptr);
								assert(FALSE);
								continue;
							} else if (cs->ins_off)
							{
								if ((cs->ins_off >
									((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
									|| (cs->ins_off < (SIZEOF(blk_hdr)
										+ SIZEOF(rec_hdr)))
									|| (0 > (short)cs->index)
									|| ((cs - cw_set_addrs) <= cs->index))
								{
									SECSHR_ACCOUNTING(7);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING((INTPTR_T)cs);
									SECSHR_ACCOUNTING(cs->blk);
									SECSHR_ACCOUNTING(cs->index);
									SECSHR_ACCOUNTING(cs->ins_off);
									SECSHR_ACCOUNTING(((blk_hdr *)blk_ptr)->bsiz);
									assert(FALSE);
									continue;
								}
								PUT_LONG((blk_ptr + cs->ins_off),
								 ((cw_set_element *)(cw_set_addrs + cs->index))->blk);
								if (((nxt = cs + 1) < cs_top)
									&& (gds_t_write_root == nxt->mode))
								{
									if ((nxt->ins_off >
									     ((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
										|| (nxt->ins_off < (SIZEOF(blk_hdr)
											 + SIZEOF(rec_hdr)))
										|| (0 > (short)nxt->index)
										|| ((cs - cw_set_addrs) <= nxt->index))
									{
										SECSHR_ACCOUNTING(7);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING((INTPTR_T)nxt);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(nxt->index);
										SECSHR_ACCOUNTING(nxt->ins_off);
										SECSHR_ACCOUNTING(
											((blk_hdr *)blk_ptr)->bsiz);
										assert(FALSE);
										continue;
									}
									PUT_LONG((blk_ptr + nxt->ins_off),
										 ((cw_set_element *)
										 (cw_set_addrs + nxt->index))->blk);
								}
							}
						} else
						{	/* TP */
							if (cs->done == 0)
							{
								if (FALSE == sec_shr_blk_build(csa, csd, is_bg, cs, blk_ptr,
												currtn))
								{
									SECSHR_ACCOUNTING(10);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING((INTPTR_T)cs);
									SECSHR_ACCOUNTING(cs->blk);
									SECSHR_ACCOUNTING(cs->level);
									SECSHR_ACCOUNTING(cs->done);
									SECSHR_ACCOUNTING(cs->forward_process);
									SECSHR_ACCOUNTING(cs->first_copy);
									SECSHR_ACCOUNTING((INTPTR_T)cs->upd_addr);
									SECSHR_ACCOUNTING((INTPTR_T)blk_ptr);
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
										SECSHR_ACCOUNTING(7);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING((INTPTR_T)cs);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(cs->index);
										SECSHR_ACCOUNTING(cs->ins_off);
										SECSHR_ACCOUNTING(
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
									   && (chain.cw_index < si->cw_set_depth)
									   && (FALSE != secshr_tp_get_cw(
									      first_cw_set, chain.cw_index, &cs_ptr)))
									{
										PUT_LONG(chain_ptr, cs_ptr->blk);
										if (0 == chain.next_off)
											break;
									} else
									{
										SECSHR_ACCOUNTING(11);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING((INTPTR_T)cs);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(cs->index);
										SECSHR_ACCOUNTING((INTPTR_T)blk_ptr);
										SECSHR_ACCOUNTING((INTPTR_T)chain_ptr);
										SECSHR_ACCOUNTING(chain.next_off);
										SECSHR_ACCOUNTING(chain.cw_index);
										SECSHR_ACCOUNTING(si->cw_set_depth);
										SECSHR_ACCOUNTING(
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
						UNIX_ONLY(
							assert((&inctn_opcode == inctn_opcode_addrs)
								&& (&inctn_detail == inctn_detail_addrs)
								&& ((inctn_bmp_mark_free_gtm == inctn_opcode)
									|| (inctn_bmp_mark_free_mu_reorg == inctn_opcode)
									|| (inctn_blkmarkfree == inctn_opcode)
									|| dse_running));
						)
						/* Check if we are freeing a V4 format block and if so decrement the
						 * blks_to_upgrd counter. Do not do this in case MUPIP REORG UPGRADE/DOWNGRADE
						 * is marking a recycled block as free (inctn_opcode is inctn_blkmarkfree).
						 */
						if ((NULL != inctn_opcode_addrs)
							&& (GTM_PROBE(SIZEOF(*inctn_opcode_addrs), inctn_opcode_addrs, READ))
							&& ((inctn_bmp_mark_free_gtm == *inctn_opcode_addrs)
								|| (inctn_bmp_mark_free_mu_reorg == *inctn_opcode_addrs))
							&& (NULL != inctn_detail_addrs)
							&& (GTM_PROBE(SIZEOF(*inctn_detail_addrs), inctn_detail_addrs, READ))
							&& (0 != inctn_detail_addrs->blknum_struct.blknum))
						{
							DECR_BLKS_TO_UPGRD(csa, csd, 1);
						}
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
						 * result in incorrect values. Hence dont touch this field if phase2.
						 */
						csd->trans_hist.free_blocks -= cs->reference_cnt;
					}
					cs->old_mode = (int4)cs->mode;
					assert(0 < cs->old_mode);
					cs->mode = gds_t_committed;	/* rolls forward step (14) */
					UNIX_ONLY(
						/* Do not do a cert_blk of bitmap here since it could give a DBBMMSTR error. The
						 * bitmap block build is COMPLETE only in wcs_recover so do the cert_blk there.
						 * Assert that the bitmap buffer will indeed go through cert_blk there.
						 */
						assert((cs->old_mode != gds_t_writemap) || !is_bg || cr->stopped);
						if (cs->old_mode != gds_t_writemap)
						{
							assert(NULL == gvt);
							CERT_BLK_IF_NEEDED(certify_all_blocks, reg, cs, blk_ptr, gvt);
						}
					)
					if (is_bg && (rundown_process_id == cr->in_tend))
					{	/* Reset cr->in_tend now that cr is uptodate. This way if at all wcs_recover
						 * sees cr->in_tend set, it can be sure that was leftover from an interrupted
						 * phase1 commit for which the complete commit happened in another cache-record
						 * which will have cr->stopped set so the in_tend cache-record can be discarded.
						 * Take this opportunity to reset data_invalid, in_cw_set and the write interlock
						 * as well thereby simulating exactly what bg_update_phase2 would have done.
						 * This is easily done in Unix using the INSQ*I macros. But in VMS, these macros
						 * will pull in extra routines (including wcs_sleep) into the privileged image
						 * GTMSECSHR which we want to avoid. Therefore in VMS, we decide to skip the
						 * part about re-inserting the dirty cache-record into the active queue.
						 * The VMS version of wcs_get_space.c needs to take this into account while
						 * it is waiting for a dirty cache-record (that it could not be in any queues).
						 */
						assert(T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit);
						if (!dont_reset_data_invalid)
							cr->data_invalid = 0;
						if (PROBE_EVEN(cr))
						{	/* Release write interlock. The following code is very similar to that
							 * at the end of the function "bg_update_phase2".
							 */
							UNIX_ONLY(
								/* Avoid using gv_cur_region in the LOCK_HIST macro that is
								 * used by the RELEASE_BUFF_UPDATE_LOCK macro by setting locknl
								 */
								locknl = cnl;
							)
							if (!cr->tn)
							{
								cr->jnl_addr = cs->jnl_freeaddr;
								assert(LATCH_SET == WRITE_LATCH_VAL(cr));
#								ifdef UNIX
								/* cache-record was not dirty BEFORE this update.
								 * insert this in the active queue. See comment above for
								 * why this is done only in Unix and not VMS.
								 */
								n = INSQTI((que_ent_ptr_t)&cr->state_que,
										(que_head_ptr_t)&cache_state->cacheq_active);
								if (INTERLOCK_FAIL == n)
								{
									SECSHR_ACCOUNTING(7);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING((INTPTR_T)cr);
									SECSHR_ACCOUNTING(cr->blk);
									SECSHR_ACCOUNTING(n);
									SECSHR_ACCOUNTING(cache_state->cacheq_active.fl);
									SECSHR_ACCOUNTING(cache_state->cacheq_active.bl);
									assert(FALSE);
								}
								ADD_ENT_TO_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
#								endif
							}
							RELEASE_BUFF_UPDATE_LOCK(cr, n, &cnl->db_latch);
							/* "n" holds the pre-release value in Unix and post-release value in VMS,
							 * so check that we did hold the lock before releasing it above */
							UNIX_ONLY(assert(LATCH_CONFLICT >= n);)
							UNIX_ONLY(assert(LATCH_CLEAR < n);)
							VMS_ONLY(assert(LATCH_SET >= n);)
							VMS_ONLY(assert(LATCH_CLEAR <= n);)
							if (WRITER_BLOCKED_BY_PROC(n))
							{
								VMS_ONLY(
									assert(LATCH_SET == WRITE_LATCH_VAL(cr));
									RELEASE_BUFF_UPDATE_LOCK(cr, n, &cnl->db_latch);
									assert(LATCH_CLEAR == n);
									assert(0 != cr->epid);
									assert(WRT_STRT_PNDNG == cr->iosb.cond);
									cr->epid = 0;
									cr->iosb.cond = 0;
									cr->wip_stopped = FALSE;
								)
#								ifdef UNIX
								n = INSQHI((que_ent_ptr_t)&cr->state_que,
										(que_head_ptr_t)&cache_state->cacheq_active);
								if (INTERLOCK_FAIL == n)
								{
									SECSHR_ACCOUNTING(7);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING((INTPTR_T)cr);
									SECSHR_ACCOUNTING(cr->blk);
									SECSHR_ACCOUNTING(n);
									SECSHR_ACCOUNTING(cache_state->cacheq_active.fl);
									SECSHR_ACCOUNTING(cache_state->cacheq_active.bl);
									assert(FALSE);
								}
#								endif
							}
						}
						assert(process_id == cr->in_cw_set);
						UNPIN_CACHE_RECORD(cr);
						assert(!cr->in_cw_set);
						SECSHR_SHM_WRITE_MEMORY_BARRIER;
						cr->in_tend = 0;
					}
				}	/* for all cw_set entries */
				/* Check if kill_in_prog flag in file header has to be incremented. */
				if (tp_update_underway)
				{	/* TP : Do this only if GT.M has not already completed the commit on this region. */
					assert((REG_COMMIT_COMPLETE == this_reg_commit_type)
						|| (REG_COMMIT_PARTIAL == this_reg_commit_type)
						|| (REG_COMMIT_UNSTARTED == this_reg_commit_type));
					/* We have already checked that "si" is READABLE. Check that it is WRITABLE since
					 * we might need to set "si->kip_csa" in the CAREFUL_INCR_KIP macro.
					 */
					if (GTM_PROBE(SIZEOF(sgm_info), si, WRITE))
					{
						kip_csa_usable = TRUE;
						/* Take this opportunity to reset si->cr_array_index */
						si->cr_array_index = 0;
					} else
					{
						kip_csa_usable = FALSE;
						assert(FALSE);
					}
					if (REG_COMMIT_COMPLETE != this_reg_commit_type)
					{
						if (kip_csa_usable && (NULL != si->kill_set_head) && (NULL == si->kip_csa))
							CAREFUL_INCR_KIP(csd, csa, si->kip_csa);
					} else
						assert((NULL == si->kill_set_head) || (NULL != si->kip_csa));
					assert((NULL == si->kill_set_head) || (NULL != si->kip_csa));
				} else
				{	/* Non-TP. Check need_kip_incr and value pointed to by kip_csa. */
					assert(non_tp_update_underway);
					/* Note that *kip_csa_addrs could be NULL if we are in the
					 * 1st phase of the M-kill and NON NULL if we are in the 2nd phase of the kill.
					 * Only if it is NULL, should we increment the kill_in_prog flag.
					 */
					kip_csa_usable =
						(GTM_PROBE(SIZEOF(*kip_csa_addrs), kip_csa_addrs, WRITE))
							? TRUE : FALSE;
					assert(kip_csa_usable);
					if (GTM_PROBE(SIZEOF(*need_kip_incr_addrs), need_kip_incr_addrs, WRITE))
						needkipincr = *need_kip_incr_addrs;
					else
					{
						needkipincr = FALSE;
						assert(FALSE);
					}
					if (needkipincr && kip_csa_usable && (NULL == *kip_csa_addrs))
					{
						CAREFUL_INCR_KIP(csd, csa, *kip_csa_addrs);
						*need_kip_incr_addrs = FALSE;
					}
#					ifdef UNIX
					if (MUSWP_INCR_ROOT_CYCLE == TREF(in_mu_swap_root_state))
						cnl->root_search_cycle++;
#					endif
				}
			}	/* if (NULL != first_cw_set) */
			/* If the process is about to exit AND any kills are in progress (bitmap freeup phase of kill), mark
			 * kill_in_prog as abandoned. Non-TP and TP maintain kill_in_prog information in different structures
			 * so access them appropriately. Note that even for a TP transaction, the bitmap freeup happens as a
			 * non-TP transaction so checking dollar_tlevel is not enough to determine if we are in TP or non-TP.
			 * Thankfully first_sgm_info is guaranteed to be non-NULL in the case of a TP transaction that is
			 * temporarily running its bitmap freeup phase as a non-TP transaction. And for true non-TP
			 * transactions, first_sgm_info is guaranteed to be NULL. So we use this for the determination.
			 * But this global variable value is obtained by dereferencing first_sgm_info_addrs (due to the way
			 * GTMSECSHR runs as a separate privileged image in VMS). If the probe of first_sgm_info_addrs does
			 * not succeed (due to some corruption), then we have no clue about the nullness of first_sgm_info.
			 * Therefore we also check for dlr_tlevel also since if that is TRUE, we are guaranteed it is a TP
			 * transaction irrespective of the value of first_sgm_info. Note that we store the value of the global
			 * variable first_sgm_info in a local variable firsgsgminfo (slightly different name) for clarity sake.
			 */
			if (is_exiting)
			{
				if (GTM_PROBE(SIZEOF(*first_sgm_info_addrs), first_sgm_info_addrs, READ))
					firstsgminfo = *first_sgm_info_addrs;
				else
				{
					assert(FALSE);
					firstsgminfo = NULL;
				}
				if (dlr_tlevel || (NULL != firstsgminfo))
				{
					si = csa->sgm_info_ptr;
					kip_csa_usable = (GTM_PROBE(SIZEOF(sgm_info), si, WRITE)) ? TRUE : FALSE;
					assert(kip_csa_usable);
					/* Since the kill process cannot be completed, we need to decerement KIP count
					 * and increment the abandoned_kills count.
					 */
					if (kip_csa_usable && (NULL != si->kill_set_head) && (NULL != si->kip_csa))
					{
						assert(csa == si->kip_csa);
						CAREFUL_DECR_KIP(csd, csa, si->kip_csa);
						CAREFUL_INCR_ABANDONED_KILLS(csd, csa);
					} else
						assert((NULL == si->kill_set_head) || (NULL == si->kip_csa));
				} else if (!dlr_tlevel)
				{
					kip_csa_usable =
						(GTM_PROBE(SIZEOF(*kip_csa_addrs), kip_csa_addrs, WRITE))
						? TRUE : FALSE;
					assert(kip_csa_usable);
					if (kip_csa_usable && (NULL != *kip_csa_addrs) && (csa == *kip_csa_addrs))
					{
						assert(0 < (*kip_csa_addrs)->hdr->kill_in_prog);
						CAREFUL_DECR_KIP(csd, csa, *kip_csa_addrs);
						CAREFUL_INCR_ABANDONED_KILLS(csd, csa);
					}
				}
			}
			if (JNL_ENABLED(csd))
			{
				if (GTM_PROBE(SIZEOF(jnl_private_control), csa->jnl, WRITE))
				{
					jbp = csa->jnl->jnl_buff;
					if (GTM_PROBE(SIZEOF(jnl_buffer), jbp, WRITE) && is_exiting)
					{
						SALVAGE_UNIX_LATCH(&jbp->fsync_in_prog_latch, is_exiting);
						if (VMS_ONLY(csa->jnl->qio_active)
							UNIX_ONLY(jbp->io_in_prog_latch.u.parts.latch_pid \
								  == rundown_process_id))
						{
							if (csa->jnl->dsk_update_inprog)
							{
								jbp->dsk = csa->jnl->new_dsk;
								jbp->dskaddr = csa->jnl->new_dskaddr;
							}
							VMS_ONLY(
								bci(&jbp->io_in_prog);
								csa->jnl->qio_active = FALSE;
							)
							UNIX_ONLY(RELEASE_SWAPLOCK(&jbp->io_in_prog_latch));
						}
						if (jbp->free_update_pid == rundown_process_id)
						{	/* Got shot in the midst of updating freeaddr/free in jnl_write.c
							 * Fix the values (possible only in VMS where we have kernel extension).
							 */
							UNIX_ONLY(assert(FALSE);)
							assert(csa->now_crit);
							jbp->free = csa->jnl->temp_free;
							jbp->freeaddr = csa->jnl->new_freeaddr;
							jbp->free_update_pid = 0;
							DBG_CHECK_JNL_BUFF_FREEADDR(jbp);
						}
						if (jbp->blocked == rundown_process_id)
						{
							assert(csa->now_crit);
							jbp->blocked = 0;
						}
					}
				} else
				{
					SECSHR_ACCOUNTING(4);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING((INTPTR_T)csa->jnl);
					SECSHR_ACCOUNTING(SIZEOF(jnl_private_control));
					assert(FALSE);
				}
			}
			if (is_exiting && csa->freeze && csd->freeze == rundown_process_id && !csa->persistent_freeze)
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
				UNIX_ONLY(
					/* cannot send oplog message in VMS as privileged routines cannot do I/O */
					send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_STR(wcblocked_ptr),
						rundown_process_id, &csd->trans_hist.curr_tn, DB_LEN_STR(reg));
				)
			}
			csa->wbuf_dqd = 0;		/* We can clear the flag now */
			if (csa->wcs_pidcnt_incremented)
				CAREFUL_DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
			if (csa->now_crit)
			{
				if (csd->trans_hist.curr_tn == csd->trans_hist.early_tn - 1)
				{	/* there can be at most one region in non-TP with different curr_tn and early_tn */
					assert(!non_tp_update_underway || first_time);
					assert(NORMAL_TERMINATION != secshr_state); /* for normal termination we should not
										     * have been in the midst of commit */
					DEBUG_ONLY(first_time = FALSE;)
					if (update_underway)
					{
						INCREMENT_CURR_TN(csd);	/* roll forward step (8) */
					} else
						csd->trans_hist.early_tn = csd->trans_hist.curr_tn;
				}
				assert(csd->trans_hist.early_tn == csd->trans_hist.curr_tn);
				if (GTM_PROBE(CRIT_SPACE(NUM_CRIT_ENTRY(csd)), csa->critical, WRITE))
				{
					/* ONLINE ROLLBACK can come here holding crit ONLY due to commit errors but NOT during
					 * process exiting as secshr_db_clnup during process exiting is always preceded by
					 * mur_close_files which does the rel_crit anyways. Assert that.
					 */
					UNIX_ONLY(assert(!csa->hold_onto_crit || !jgbl.onlnrlbk || !is_exiting));
					if (!csa->hold_onto_crit || is_exiting)
					{ 	/* Release crit but since it involves modifying more than one field, make sure
						 * we prevent interrupts while in this code. The global variable "crit_count"
						 * does this for us. See similar usage in rel_crit.c. We currently use this here
						 * only for Unix because in VMS, a global variable in GTMSHR is not accessible
						 * in GTMSECSHR image easily unless passed through init_secshr_addrs. Since in
						 * VMS, if we are here, we are already in a kernel level routine, we will not be
						 * interrupted by user level timer handlers (wcs_stale or wcs_clean_dbsync_ast)
						 * that care about the consistency of the crit values so it is okay not to
						 * explicitly prevent interrupts using "crit_count" in VMS.
						 */
						UNIX_ONLY(
							assert(0 == crit_count);
							crit_count++;	/* prevent interrupts */
							CRIT_TRACE(crit_ops_rw); /* see gdsbt.h for comment on placement */
						)
						if (cnl->in_crit == rundown_process_id)
							cnl->in_crit = 0;
						UNIX_ONLY(
							csa->hold_onto_crit = FALSE;
							DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
							mutex_unlockw(reg, crash_count);/* roll forward step (11) */
							assert(!csa->now_crit);
							DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
							crit_count = 0;
						)
						VMS_ONLY(
							mutex_stoprelw(csa->critical);	/* roll forward step (11) */
							csa->now_crit = FALSE;
						)
						UNSUPPORTED_PLATFORM_CHECK;
					}
				} else
				{
					SECSHR_ACCOUNTING(6);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING((INTPTR_T)cnl);
					SECSHR_ACCOUNTING(NODE_LOCAL_SIZE_DBS);
					SECSHR_ACCOUNTING((INTPTR_T)csa->critical);
					SECSHR_ACCOUNTING(CRIT_SPACE(NUM_CRIT_ENTRY(csd)));
					assert(FALSE);
				}
			}
			csa->t_commit_crit = FALSE; /* ensure we don't process this region again (rolls forward step (15)) */
			if ((NORMAL_TERMINATION == secshr_state || ABNORMAL_TERMINATION == secshr_state)
			    && GTM_PROBE(SHMPOOL_BUFFER_SIZE, csa->shmpool_buffer, WRITE))
			{
				if ((pid = csa->shmpool_buffer->shmpool_crit_latch.u.parts.latch_pid)
				    == rundown_process_id VMS_ONLY(&&)
				    VMS_ONLY((imgcnt = csa->shmpool_buffer->shmpool_crit_latch.u.parts.latch_image_count) \
					     == rundown_image_count))
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
					if (COMPSWAP_LOCK(&csa->shmpool_buffer->shmpool_crit_latch, pid, imgcnt,
							  rundown_process_id, rundown_image_count))
					{	/* Now our lock .. set blocked and release.  */
						csa->shmpool_buffer->shmpool_blocked = TRUE;
						BG_TRACE_PRO_ANY(csa, shmpool_blkd_by_sdc);
						DEBUG_LATCH(util_out_print("Orphaned latch cleaned up", TRUE));
						COMPSWAP_UNLOCK(&csa->shmpool_buffer->shmpool_crit_latch, rundown_process_id,
								rundown_image_count, LOCK_AVAILABLE, 0);
					} /* Else someone else took care of it */
				}
			}
#ifdef UNIX
			/* All releases done now. Double check latch is really cleared */
			if (GTM_PROBE(CRIT_SPACE(NUM_CRIT_ENTRY(csd)), csa->critical, WRITE))
			{
				/* as long as csa->hold_onto_crit is FALSE, we should have released crit if we held it at entry */
				assert(!csa->now_crit || csa->hold_onto_crit);
				SALVAGE_UNIX_LATCH_DBCRIT(&csa->critical->semaphore, is_exiting, cnl->wc_blocked);
				SALVAGE_UNIX_LATCH(&csa->critical->crashcnt_latch, is_exiting);
				SALVAGE_UNIX_LATCH(&csa->critical->prochead.latch, is_exiting);
				SALVAGE_UNIX_LATCH(&csa->critical->freehead.latch, is_exiting);
			}
#endif
		}	/* For all regions */
	}	/* For all glds */
	if (jnlpool_reg_addrs && (GTM_PROBE(SIZEOF(*jnlpool_reg_addrs), jnlpool_reg_addrs, READ)))
	{	/* although there is only one jnlpool reg, SECSHR_PROBE_REGION macro might do a "continue" and hence the for loop */
		for (reg = *jnlpool_reg_addrs, jnlpool_reg = TRUE; jnlpool_reg && reg; jnlpool_reg = FALSE) /* only jnlpool reg */
		{
			SECSHR_PROBE_REGION(reg);	/* SECSHR_PROBE_REGION sets csa */
			if (csa->now_crit)
			{
				jpl = (jnlpool_ctl_ptr_t)((sm_uc_ptr_t)csa->critical - JNLPOOL_CTL_SIZE); /* see jnlpool_init() for
													   * relationship between
													   * critical and jpl */
				if (GTM_PROBE(SIZEOF(jnlpool_ctl_struct), jpl, WRITE))
				{
					if ((jpl->early_write_addr > jpl->write_addr) && (update_underway))
					{	/* we need to update journal pool to reflect the increase in jnl-seqno */
						cumul_jnl_rec_len = (uint4)(jpl->early_write_addr - jpl->write_addr);
						jh = (jnldata_hdr_ptr_t)((sm_uc_ptr_t)jpl + JNLDATA_BASE_OFF + jpl->write);
						if (GTM_PROBE(SIZEOF(*jh), jh, WRITE) && 0 != (jsize = jpl->jnlpool_size))
						{	/* Below chunk of code mirrors  what is done in t_end/tp_tend */
							/* Begin atomic stmnts. Follow same order as in t_end/tp_tend */
							jh->jnldata_len = cumul_jnl_rec_len;
							jh->prev_jnldata_len = jpl->lastwrite_len;
#							ifdef UNIX
							if (INVALID_SUPPL_STRM != strm_index)
							{	/* Need to also update supplementary stream seqno */
								assert(0 <= strm_index);
								/* assert(strm_index < ARRAYSIZE(tjpl->strm_seqno)); */
								ASSERT_INST_FILE_HDR_HAS_HISTREC_FOR_STRM(strm_index);
								jpl->strm_seqno[strm_index]++;
							}
#							endif
							jpl->lastwrite_len = cumul_jnl_rec_len;
							SECSHR_SHM_WRITE_MEMORY_BARRIER;
							/* Emulate
							 * jpl->write = (jpl->write + cumul_jnl_rec_len) % jsize;
							 * See note in DOs and DONTs about using % operator
							 */
							for (new_write = jpl->write + cumul_jnl_rec_len;
								new_write >= jsize;
								new_write -= jsize)
								;
							jpl->write = new_write;
							jpl->write_addr += cumul_jnl_rec_len;
							jpl->jnl_seqno++;
							/* End atomic stmts */
							/* the above takes care of rolling forward steps (9) and (10) of the
							 * commit flow */
						}
					}
#ifdef DEBUG
					else if (jpl->early_write_addr > jpl->write_addr)
					{   /* PRO code will do the right thing by overwriting that exact space in the jnlpool with
					     * the current transaction's journal records. For dbg though, it is better if
					     * secshr_db_clnup (which is invoked as part of exit handling) does the cleanup.
					     */
						assert(!update_underway);
						jpl->early_write_addr = jpl->write_addr;
					}
#endif
				}
				cnl = csa->nl;
				if ((GTM_PROBE(NODE_LOCAL_SIZE_DBS, cnl, WRITE)) &&
					(GTM_PROBE(JNLPOOL_CRIT_SPACE, csa->critical, WRITE)))
				{
					/* ONLINE ROLLBACK can come here holding crit ONLY due to commit errors but NOT during
					 * process exiting as secshr_db_clnup during process exiting is always preceded by
					 * mur_close_files which does the rel_crit anyways. Assert that.
					 */
					UNIX_ONLY(assert(!csa->hold_onto_crit || !jgbl.onlnrlbk || !is_exiting));
					if (!csa->hold_onto_crit || is_exiting)
					{
						UNIX_ONLY(CRIT_TRACE(crit_ops_rw)); /* see gdsbt.h for comment on placement */
						if (cnl->in_crit == rundown_process_id)
							cnl->in_crit = 0;
						UNIX_ONLY(
							csa->hold_onto_crit = FALSE;
							DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
							mutex_unlockw(reg, 0);		/* roll forward step (12) */
							assert(!csa->now_crit);
							DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
						)
						VMS_ONLY(
							mutex_stoprelw(csa->critical);	/* roll forward step (12) */
							csa->now_crit = FALSE;
						)
						/* the above takes care of rolling forward step (12) of the commit flow */
					}
				}
			}
			/* as long as csa->hold_onto_crit is FALSE, we should have released crit if we held it at entry */
			UNIX_ONLY(assert(!csa->now_crit || csa->hold_onto_crit));
		}
	}
	return;
}

boolean_t	secshr_tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1)
{
	int	iter;

	*cs1 = cs;
	for (iter = 0; iter < depth; iter++)
	{
		if (!(GTM_PROBE(SIZEOF(cw_set_element), *cs1, READ)))
		{
			*cs1 = NULL;
			return FALSE;
		}
		*cs1 = (*cs1)->next_cw_set;
	}
	if (*cs1 && GTM_PROBE(SIZEOF(cw_set_element), *cs1, READ))
	{
		while ((*cs1)->high_tlevel)
		{
			if (GTM_PROBE(SIZEOF(cw_set_element), (*cs1)->high_tlevel, READ))
				*cs1 = (*cs1)->high_tlevel;
			else
			{
				*cs1 = NULL;
				return FALSE;
			}
		}
	}
	return TRUE;
}
