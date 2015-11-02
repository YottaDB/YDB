/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#endif
#include "sec_shr_blk_build.h"
#include "sec_shr_map_build.h"
#include "add_inter.h"
#include "send_msg.h"	/* for send_msg prototype */
#include "secshr_db_clnup.h"
#include "gdsbgtr.h"
#include "memcoherency.h"
#include "shmpool.h"

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

#define	WCBLOCKED_WBUF_DQD_LIT	"wcb_secshr_db_clnup_wbuf_dqd"
#define	WCBLOCKED_NOW_CRIT_LIT	"wcb_secshr_db_clnup_now_crit"

/* SECSHR_ACCOUNTING macro assumes csd is dereferencible and uses "csa", "csd" and "is_bg" */
#define		SECSHR_ACCOUNTING(value)						\
{											\
	if (do_accounting)								\
	{										\
		if (csd->secshr_ops_index < sizeof(csd->secshr_ops_array))		\
			csd->secshr_ops_array[csd->secshr_ops_index] = (uint4)(value);	\
		csd->secshr_ops_index++;						\
	}										\
}

/* IMPORTANT : SECSHR_PROBE_REGION sets csa */
#define	SECSHR_PROBE_REGION(reg)									\
	if (!GTM_PROBE(sizeof(gd_region), (reg), READ))							\
		continue; /* would be nice to notify the world of a problem but where and how?? */	\
	if (!reg->open || reg->was_open)								\
		continue;										\
	if (!GTM_PROBE(sizeof(gd_segment), (reg)->dyn.addr, READ))					\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	if ((dba_bg != (reg)->dyn.addr->acc_meth) && (dba_mm != (reg)->dyn.addr->acc_meth))		\
		continue;										\
	if (!GTM_PROBE(sizeof(file_control), (reg)->dyn.addr->file_cntl, READ))				\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	if (!GTM_PROBE(sizeof(GDS_INFO), (reg)->dyn.addr->file_cntl->file_info, READ))			\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	csa = &(FILE_INFO((reg)))->s_addrs;								\
	if (!GTM_PROBE(sizeof(sgmnt_addrs), csa, WRITE))						\
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
#  define CHECK_UNIX_LATCH(X, is_exiting)
#else
#  define CHECK_UNIX_LATCH(X, is_exiting) 	CHECK_LATCH(X, is_exiting)
GBLREF pid_t	process_id;	/* Used in xxx_SWAPLOCK macros .. has same value as rundown_process_id on UNIX */
#endif

#define CHECK_LATCH(X, is_exiting)							\
{											\
	uint4 pid;									\
									                \
	if ((pid = (X)->u.parts.latch_pid) == rundown_process_id)				\
	{										\
		if (is_exiting)								\
		{									\
			SET_LATCH_GLOBAL(X, LOCK_AVAILABLE);				\
			DEBUG_LATCH(util_out_print("Latch cleaned up", FLUSH));		\
		}									\
	} else if (0 != pid && FALSE == is_proc_alive(pid, UNIX_ONLY(0) VMS_ONLY((X)->u.parts.latch_image_count)))	\
	{										\
		  DEBUG_LATCH(util_out_print("Orphaned latch cleaned up", TRUE));	\
		  COMPSWAP((X), pid, (X)->u.parts.latch_image_count, LOCK_AVAILABLE, 0);	\
	}										\
}

GBLDEF gd_addr		*(*get_next_gdr_addrs)();
GBLDEF cw_set_element	*cw_set_addrs;
GBLDEF sgm_info		**first_sgm_info_addrs;
GBLDEF unsigned char	*cw_depth_addrs;
GBLDEF uint4		rundown_process_id;
GBLDEF uint4		rundown_image_count;
GBLDEF int4		rundown_os_page_size;
GBLDEF gd_region	**jnlpool_reg_addrs;

#ifdef UNIX
GBLREF short		crash_count;
GBLREF node_local_ptr_t	locknl;
#endif

bool secshr_tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);

void secshr_db_clnup(enum secshr_db_state secshr_state)
{
	unsigned char		*chain_ptr;
	char			*wcblocked_ptr;
	boolean_t		is_bg, jnlpool_reg, do_accounting, first_time = TRUE, is_exiting;
	boolean_t		tp_update_underway = FALSE;	/* set to TRUE if TP commit was in progress or complete */
	boolean_t		non_tp_update_underway = FALSE;	/* set to TRUE if non-TP commit was in progress or complete */
	boolean_t		update_underway = FALSE;	/* set to TRUE if either TP or non-TP commit was underway */
	int			max_bts;
	unsigned int		lcnt;
	cache_rec_ptr_t		clru, cr, cr_top, start_cr;
	cw_set_element		*cs, *cs_ptr, *cs_top, *first_cw_set, *nxt, *orig_cs;
	gd_addr			*gd_header;
	gd_region		*reg, *reg_top;
	jnl_buffer_ptr_t	jbp;
	off_chain		chain;
	sgm_info		*si;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_ptr;
	jnlpool_ctl_ptr_t	jpl;
	jnldata_hdr_ptr_t	jh;
	uint4			cumul_jnl_rec_len, jsize, new_write, imgcnt;
	pid_t			pid;

	error_def(ERR_WCBLOCKED);

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
	 * Instead it sets csd->wc_blocked to TRUE thereby ensuring the next process that gets CRIT does a cache recovery
	 * 	which will take care of doing more than the ROLL-BACK that t_commit_cleanup would have otherwise done.
	 *
	 * The logic for determining if it is a ROLL-BACK or ROLL-FORWARD is explained below.
	 * The commit logic flow in tp_tend and t_end can be captured as follows. Note that in t_end there is only one region.
	 *
	 *  1) Get crit on all regions
	 *  2) Get crit on jnlpool
	 *  3) jnlpool_ctl->early_write_addr += delta;
	 *       For each region participating
	 *       {
	 *  4)     csd->trans_hist.early_tn++;
	 *         Write journal records
	 *  5)     csa->hdr->reg_seqno = jnlpool_ctl->jnl_seqno + 1;
	 *       }
	 *       For each region participating
	 *       {
	 *  6)	    csa->t_commit_crit = TRUE;
	 *             For every cw-set-element of this region
	 *             {
	 *               Commit this particular block.
	 *               cs->mode = gds_t_committed;
	 *             }
	 *  7)       csa->t_commit_crit = FALSE;
	 *  8)     csd->trans_hist.curr_tn++;
	 *       }
	 *  9) jnlpool_ctl->write_addr = jnlpool_ctl->early_write_addr;
	 * 10) jnlpool_ctl->jnl_seqno++;
	 * 11) Release crit on jnlpool
	 * 12) Release crit on and all regions
	 *
	 * If a TP transaction has proceeded to step (6) for at least one region, then "tp_update_underway" is set to TRUE
	 * and the transaction cannot be rolled back but has to be committed. Otherwise the transaction is rolled back.
	 *
	 * If a non-TP transaction has proceeded to step (6), then "non_tp_update_underway" is set to TRUE
	 * and the transaction cannot be rolled back but has to be committed. Otherwise the transaction is rolled back.
	 *
	 * There is "one exception" though. A non-TP transaction that does a duplicate set (i.e. has cw_set_depth = 0 although
	 * update_trans is TRUE), WILL not set "non_tp_update_underway" to TRUE even though it has proceeded to step (7). This
	 * is because there will be no cw_set_element that is updated to see that cs->mode is gds_t_committed. One can do
	 * something similar to what is done for TP for duplicate sets which is to set si->update_trans to a special value
	 * T_COMMIT_STARTED that will indicate this fact to secshr_db_clnup/t_commit_cleanup. But that unfortunately can
	 * only be done through the global variable "update_trans" for non-TP and that is not accessible by secshr_db_clnup
	 * unless it is passed as another parameter in init_secshr_addrs. Since that involves changing a whole lot of things
	 * than is worth it, we temporarily roll-back a duplicate set non-TP update even though it has proceeded to step (7).
	 *
	 * There is an "exception to this exception" though and that is if secshr_db_clnup is called from t_commit_cleanup,
	 * it will be called with secshr_state == COMMIT_INCOMPLETE and in that case we do not need access to "update_trans"
	 * to determine the fact that we are past step (6). This is because t_commit_cleanup (which had access to "update_trans")
	 * has already determined we are past step (6) before calling secshr_db_clnup and so we can rely on that.
	 */
	is_exiting = (ABNORMAL_TERMINATION == secshr_state) || (NORMAL_TERMINATION == secshr_state);
	if (GTM_PROBE(sizeof(first_sgm_info_addrs), first_sgm_info_addrs, READ))
	{	/* determine update_underway for TP transaction */
		for (si = *first_sgm_info_addrs;  NULL != si;  si = si->next_sgm_info)
		{
			if (GTM_PROBE(sizeof(sgm_info), si, READ))
			{
				if (GTM_PROBE(sizeof(cw_set_element), si->first_cw_set, READ))
				{	/* Note that SECSHR_PROBE_REGION does a "continue" if any probes fail. */
					SECSHR_PROBE_REGION(si->gv_cur_region);	/* sets csa */
					/* Set update_underway to TRUE only if we have crit on this region. */
					if (csa->now_crit
						&& (csa->t_commit_crit
							|| (si->cw_set_depth && (gds_t_committed == si->first_cw_set->mode))))
					{
						tp_update_underway = TRUE;
						update_underway = TRUE;
					}
					break;
				} else if (si->update_trans)
				{	/* case of duplicate set not creating any cw-sets but updating db curr_tn++ */
					SECSHR_PROBE_REGION(si->gv_cur_region);	/* sets csa */
					/* Set update_underway to TRUE only if we have crit on this region. */
					if (csa->now_crit && (T_COMMIT_STARTED == si->update_trans))
					{
						tp_update_underway = TRUE;
						update_underway = TRUE;
					}
					break;
				}
			} else
				break;
		}
	}
	if (GTM_PROBE(sizeof(unsigned char), cw_depth_addrs, READ))
	{	/* determine update_underway for non-TP transaction */
		/* we assume here that cw_set_depth is reset to 0 in t_end once an active non-TP transaction is complete */
		if (0 != *cw_depth_addrs)
		{
			assert(!tp_update_underway);
			assert(!update_underway);
			cs = cw_set_addrs;
			if (GTM_PROBE(sizeof(cw_set_element), cs, READ))
			{
				if (gds_t_committed == cs->mode)
				{
					non_tp_update_underway = TRUE;	/* non-tp update was underway */
					update_underway = TRUE;
				}
			}
		}
	}
	if (COMMIT_INCOMPLETE == secshr_state && !tp_update_underway)
	{	/* if we are called from t_commit_cleanup and we have determined above that tp_update_underway is not TRUE,
		 * then there should have been a non-TP update underway (otherwise t_commit_cleanup would not have called us)
		 * this takes care of the comment above which talks about "exception to this exception".
		 */
		non_tp_update_underway = TRUE;	/* non-tp update was underway */
		update_underway = TRUE;
	}
	for (gd_header = (*get_next_gdr_addrs)(NULL);  NULL != gd_header;  gd_header = (*get_next_gdr_addrs)(gd_header))
	{
		if (!GTM_PROBE(sizeof(gd_addr), gd_header, READ))
			break;	/* if gd_header is accessible */
		for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions;  reg < reg_top;  reg++)
		{
			SECSHR_PROBE_REGION(reg);	/* SECSHR_PROBE_REGION sets csa */
			csd = csa->hdr;
			if (!GTM_PROBE(sizeof(sgmnt_data), csd, WRITE))
				continue; /* would be nice to notify the world of a problem but where and how? */
			is_bg = (csd->acc_meth == dba_bg);
			do_accounting = FALSE;
			/* do SECSHR_ACCOUNTING only if holding crit (to avoid another process' normal termination call
			 * to secshr_db_clnup from overwriting whatever important information we wrote. if we are in
			 * crit, for the next process to overwrite us it needs to get crit which in turn will invoke
			 * wcs_recover which in turn will send whatever we wrote to the operator log).
			 * also cannot update csd if MM and read-only. take care of that too. */
			if (csa->now_crit && (csa->read_write || is_bg))
			{	/* start accounting */
				csd->secshr_ops_index = 0;
				do_accounting = TRUE;
			}
			SECSHR_ACCOUNTING(4);	/* 4 is the number of arguments following including self */
			SECSHR_ACCOUNTING(__LINE__);
			SECSHR_ACCOUNTING(rundown_process_id);
			SECSHR_ACCOUNTING(secshr_state);
			if (csa->ti != &csd->trans_hist)
			{
				SECSHR_ACCOUNTING(4);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING(csa->ti);
				SECSHR_ACCOUNTING(&csd->trans_hist);
				csa->ti = &csd->trans_hist;	/* better to correct and proceed than to stop */
			}
			SECSHR_ACCOUNTING(3);	/* 3 is the number of arguments following including self */
			SECSHR_ACCOUNTING(__LINE__);
			SECSHR_ACCOUNTING(csd->trans_hist.curr_tn);
			if (GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE) && is_exiting)
			{
				/* If we hold any latches in the node_local area, release them. Note we do not check
				   db_latch here because it is never used by the compare and swap logic but rather
				   the aswp logic. Since it is only used for the 3 state cache record lock and
				   separate recovery exists for it, we do not do anything with it here.
				*/
				CHECK_UNIX_LATCH(&csa->nl->wc_var_lock, is_exiting);
				if (ABNORMAL_TERMINATION == secshr_state)
				{
					if (csa->timer)
					{
						if (-1 < csa->nl->wcs_timers) /* private flag is optimistic: dont overdo */
							CAREFUL_DECR_CNT(&csa->nl->wcs_timers, &csa->nl->wc_var_lock);
						csa->timer = FALSE;
					}
					if (csa->read_write && csa->ref_cnt)
					{
						assert(0 < csa->nl->ref_cnt);
						csa->ref_cnt--;
						assert(!csa->ref_cnt);
						CAREFUL_DECR_CNT(&csa->nl->ref_cnt, &csa->nl->wc_var_lock);
					}
				}
				if ((csa->in_wtstart) && (0 < csa->nl->in_wtstart))
					CAREFUL_DECR_CNT(&csa->nl->in_wtstart, &csa->nl->wc_var_lock);
				csa->in_wtstart = FALSE;	/* Let wcs_wtstart run for exit processing */
				if (csa->nl->wcsflu_pid == rundown_process_id)
					csa->nl->wcsflu_pid = 0;
			}
			if (is_bg)
			{
				if ((0 == reg->sec_size) || !GTM_PROBE(reg->sec_size VMS_ONLY(* OS_PAGELET_SIZE), csa->nl, WRITE))
				{
					SECSHR_ACCOUNTING(3);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(reg->sec_size VMS_ONLY(* OS_PAGELET_SIZE));
					continue;
				}
				CHECK_UNIX_LATCH(&csa->acc_meth.bg.cache_state->cacheq_active.latch, is_exiting);
				start_cr = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
				max_bts = csd->n_bts;
				if (!GTM_PROBE(max_bts * sizeof(cache_rec), start_cr, WRITE))
				{
					SECSHR_ACCOUNTING(3);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(start_cr);
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
						CHECK_UNIX_LATCH(&cr->rip_latch, is_exiting);
						assert(rundown_process_id);
						if ((cr->r_epid == rundown_process_id) && (0 == cr->dirty)
								&& (FALSE == cr->in_cw_set))
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
			if (tp_update_underway)
			{	/* this is contructed to deal with the issue of reg != si->gv_cur_region
				 * due to the possibility of multiple global directories pointing to regions
				 * that resolve to the same physical file; was_open prevents processing the segment
				 * more than once, so this code matches on the file rather than the region to make sure
				 * that it gets processed at least once */
				for (si = *first_sgm_info_addrs;  NULL != si;  si = si->next_sgm_info)
				{
					if (!GTM_PROBE(sizeof(sgm_info), si, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(si);
						break;
					} else if (!GTM_PROBE(sizeof(gd_region), si->gv_cur_region, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(si->gv_cur_region);
						continue;
					} else if (!GTM_PROBE(sizeof(gd_segment), si->gv_cur_region->dyn.addr, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(si->gv_cur_region->dyn.addr);
						continue;
					} else if (si->gv_cur_region->dyn.addr->file_cntl == reg->dyn.addr->file_cntl)
					{
						cs = si->first_cw_set;
						if (cs && GTM_PROBE(sizeof(cw_set_element), cs, READ))
						{
							while (cs->high_tlevel)
							{
								if (GTM_PROBE(sizeof(cw_set_element),
											cs->high_tlevel, READ))
									cs = cs->high_tlevel;
								else
								{
									SECSHR_ACCOUNTING(3);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING(cs->high_tlevel);
									first_cw_set = cs = NULL;
									break;
								}
							}
						}
						first_cw_set = cs;
						break;
					}
				}
				if (NULL == si)
				{
					SECSHR_ACCOUNTING(2);
					SECSHR_ACCOUNTING(__LINE__);
				}
			} else if (csa->t_commit_crit)
			{	/* We better have held crit on this region. GTMASSERT only in Unix as this module runs
				 * in kernel mode in VMS and no IO is allowed in that mode.
				 */
				if (!csa->now_crit)
				{
					UNIX_ONLY(GTMASSERT;)
					VMS_ONLY(assert(FALSE);)
				}
				if (!GTM_PROBE(sizeof(unsigned char), cw_depth_addrs, READ))
				{
					SECSHR_ACCOUNTING(3);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(cw_depth_addrs);
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
					non_tp_update_underway = TRUE;
					update_underway = TRUE;
				}
			}
			/* It is possible that we were in the midst of a non-TP commit for this region at or past stage (7),
			 * with csa->t_commit_crit set to FALSE. It is a case of duplicate SET with zero cw_set_depth.
			 * See comment at beginning of module about commit logic flow and the non-TP "one exception"
			 */
			if (NULL != first_cw_set)
			{
				assert(non_tp_update_underway || tp_update_underway);
				/* Now that we have decided this region has a few cw-set-elements to be rolled forward,
				 * we better have held crit on this region. GTMASSERT only in Unix as this module runs
				 * in kernel mode in VMS and no IO is allowed in that mode.
				 */
				if (!csa->now_crit)
				{
					UNIX_ONLY(GTMASSERT;)
					VMS_ONLY(assert(FALSE);)
				}
				if (is_bg)
				{
					clru = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, csa->nl->cur_lru_cache_rec_off);
					lcnt = 0;
				}
				if (csa->t_commit_crit)
				{
					assert(NORMAL_TERMINATION != secshr_state); /* for normal termination we should not
										     * have been in the midst of commit */
					csd->trans_hist.free_blocks = csa->prev_free_blks;
				}
				SECSHR_ACCOUNTING(tp_update_underway ? 6 : 7);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING(first_cw_set);
				SECSHR_ACCOUNTING(tp_update_underway);
				SECSHR_ACCOUNTING(non_tp_update_underway);
				if (!tp_update_underway)
				{
					SECSHR_ACCOUNTING(cs_top);
					SECSHR_ACCOUNTING(*cw_depth_addrs);
				} else
					SECSHR_ACCOUNTING(si->cw_set_depth);
				for (; (tp_update_underway  &&  NULL != cs) || (!tp_update_underway  &&  cs < cs_top);
					cs = tp_update_underway ? orig_cs->next_cw_set : (cs + 1))
				{
					if (tp_update_underway)
					{
						orig_cs = cs;
						if (cs && GTM_PROBE(sizeof(cw_set_element), cs, READ))
						{
							while (cs->high_tlevel)
							{
								if (GTM_PROBE(sizeof(cw_set_element),
											cs->high_tlevel, READ))
									cs = cs->high_tlevel;
								else
								{
									SECSHR_ACCOUNTING(3);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING(cs->high_tlevel);
									cs = NULL;
									break;
								}
							}
						}
					}
					if (!GTM_PROBE(sizeof(cw_set_element), cs, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(cs);
						break;
					}
					if (csa->t_commit_crit && 0 != cs->reference_cnt)
						csd->trans_hist.free_blocks -= cs->reference_cnt;
					if (gds_t_write_root == cs->mode)
						continue; /* processing the previous cs automatically takes care of this */
					if (gds_t_committed == cs->mode)
					{	/* already processed */
						cr = cs->cr;
						if (!GTM_PROBE(sizeof(cache_rec), cr, WRITE))
						{
							SECSHR_ACCOUNTING(4);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(cs);
							SECSHR_ACCOUNTING(cr);
						} else if (cr->in_tend)
						{	/* We should have been shot (STOP/ID) in the window of two
							 * statements in bg_update immediately after cs->mode got set to
							 * gds_t_committed but just before cr->in_tend got reset to FALSE.
							 * Complete the cr->in_tend reset and skip processing this cs
							 */
							UNIX_ONLY(assert(FALSE);)
							assert(ABNORMAL_TERMINATION == secshr_state);
							cr->in_tend = FALSE;
						}
						continue;
					}
					assert(NORMAL_TERMINATION != secshr_state); /* for normal termination we should not
										     * have been in the midst of commit */
					if (is_bg)
					{
						for (; lcnt++ < max_bts;)
						{	/* find any available cr */
							if (clru++ >= cr_top)
								clru = start_cr;
							if (((0 == clru->dirty) && (FALSE == clru->in_cw_set))
								&& (-1 == clru->read_in_progress)
								&& (GTM_PROBE(csd->blk_size,
									GDS_ANY_REL2ABS(csa, clru->buffaddr), WRITE)))
								break;
						}
						if (lcnt >= max_bts)
						{
							SECSHR_ACCOUNTING(9);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(cs);
							SECSHR_ACCOUNTING(cs->blk);
							SECSHR_ACCOUNTING(cs->tn);
							SECSHR_ACCOUNTING(cs->level);
							SECSHR_ACCOUNTING(cs->done);
							SECSHR_ACCOUNTING(cs->forward_process);
							SECSHR_ACCOUNTING(cs->first_copy);
							continue;
						}
						cr = clru++;
						cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
						cr->blk = cs->blk;
						cr->jnl_addr = cs->jnl_freeaddr;
						cr->stopped = TRUE;
						/* the following code is very similar to that in bg_update */
						if (gds_t_acquired == cs->mode)
						{
							cr->ondsk_blkver = csd->desired_db_format;
							if (GDSV4 == csd->desired_db_format)
							{
								INCR_BLKS_TO_UPGRD(csa, csd, 1);
							}
						} else
						{
							cr->ondsk_blkver = cs->ondsk_blkver;
							if (cr->ondsk_blkver != csd->desired_db_format)
							{
								cr->ondsk_blkver = csd->desired_db_format;
								if (GDSV4 == csd->desired_db_format)
								{
									INCR_BLKS_TO_UPGRD(csa, csd, 1);
								} else
								{
									DECR_BLKS_TO_UPGRD(csa, csd, 1);
								}
							}
						}
						blk_ptr = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
					} else
					{
						blk_ptr = (sm_uc_ptr_t)csa->acc_meth.mm.base_addr + csd->blk_size * cs->blk;
						if (!GTM_PROBE(csd->blk_size, blk_ptr, WRITE))
						{
							SECSHR_ACCOUNTING(7);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(cs);
							SECSHR_ACCOUNTING(cs->blk);
							SECSHR_ACCOUNTING(blk_ptr);
							SECSHR_ACCOUNTING(csd->blk_size);
							SECSHR_ACCOUNTING(csa->acc_meth.mm.base_addr);
							continue;
						}
					}
					if (cs->mode == gds_t_writemap)
					{
						if (!GTM_PROBE(csd->blk_size, cs->old_block, READ))
						{
							SECSHR_ACCOUNTING(11);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(cs);
							SECSHR_ACCOUNTING(cs->blk);
							SECSHR_ACCOUNTING(cs->tn);
							SECSHR_ACCOUNTING(cs->level);
							SECSHR_ACCOUNTING(cs->done);
							SECSHR_ACCOUNTING(cs->forward_process);
							SECSHR_ACCOUNTING(cs->first_copy);
							SECSHR_ACCOUNTING(cs->old_block);
							SECSHR_ACCOUNTING(csd->blk_size);
							continue;
						}
						memmove(blk_ptr, cs->old_block, csd->blk_size);
						if (FALSE == sec_shr_map_build((uint4*)cs->upd_addr, blk_ptr, cs,
							csd->trans_hist.curr_tn, BM_SIZE(csd->bplmap)))
						{
							SECSHR_ACCOUNTING(11);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(cs);
							SECSHR_ACCOUNTING(cs->blk);
							SECSHR_ACCOUNTING(cs->tn);
							SECSHR_ACCOUNTING(cs->level);
							SECSHR_ACCOUNTING(cs->done);
							SECSHR_ACCOUNTING(cs->forward_process);
							SECSHR_ACCOUNTING(cs->first_copy);
							SECSHR_ACCOUNTING(cs->upd_addr);
							SECSHR_ACCOUNTING(blk_ptr);
						}
					} else
					{
						if (!tp_update_underway)
						{
							if (FALSE == sec_shr_blk_build(csa, csd, is_bg,
										cs, blk_ptr, csd->trans_hist.curr_tn))
							{
								SECSHR_ACCOUNTING(10);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING(cs);
								SECSHR_ACCOUNTING(cs->blk);
								SECSHR_ACCOUNTING(cs->level);
								SECSHR_ACCOUNTING(cs->done);
								SECSHR_ACCOUNTING(cs->forward_process);
								SECSHR_ACCOUNTING(cs->first_copy);
								SECSHR_ACCOUNTING(cs->upd_addr);
								SECSHR_ACCOUNTING(blk_ptr);
								continue;
							} else if (cs->ins_off)
							{
								if ((cs->ins_off >
									((blk_hdr *)blk_ptr)->bsiz - sizeof(block_id))
									|| (cs->ins_off < (sizeof(blk_hdr)
										+ sizeof(rec_hdr)))
									|| (0 > (short)cs->index)
									|| ((cs - cw_set_addrs) <= cs->index))
								{
									SECSHR_ACCOUNTING(7);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING(cs);
									SECSHR_ACCOUNTING(cs->blk);
									SECSHR_ACCOUNTING(cs->index);
									SECSHR_ACCOUNTING(cs->ins_off);
									SECSHR_ACCOUNTING(((blk_hdr *)blk_ptr)->bsiz);
									continue;
								}
								PUT_LONG((blk_ptr + cs->ins_off),
								 ((cw_set_element *)(cw_set_addrs + cs->index))->blk);
								if (((nxt = cs + 1) < cs_top)
									&& (gds_t_write_root == nxt->mode))
								{
									if ((nxt->ins_off >
									     ((blk_hdr *)blk_ptr)->bsiz - sizeof(block_id))
										|| (nxt->ins_off < (sizeof(blk_hdr)
											 + sizeof(rec_hdr)))
										|| (0 > (short)nxt->index)
										|| ((cs - cw_set_addrs) <= nxt->index))
									{
										SECSHR_ACCOUNTING(7);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING(nxt);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(nxt->index);
										SECSHR_ACCOUNTING(nxt->ins_off);
										SECSHR_ACCOUNTING(
											((blk_hdr *)blk_ptr)->bsiz);
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
												csd->trans_hist.curr_tn))
								{
									SECSHR_ACCOUNTING(10);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING(cs);
									SECSHR_ACCOUNTING(cs->blk);
									SECSHR_ACCOUNTING(cs->level);
									SECSHR_ACCOUNTING(cs->done);
									SECSHR_ACCOUNTING(cs->forward_process);
									SECSHR_ACCOUNTING(cs->first_copy);
									SECSHR_ACCOUNTING(cs->upd_addr);
									SECSHR_ACCOUNTING(blk_ptr);
									continue;
								}
								if (cs->ins_off != 0)
								{
									if ((cs->ins_off
										> ((blk_hdr *)blk_ptr)->bsiz
											- sizeof(block_id))
										|| (cs->ins_off
										 < (sizeof(blk_hdr) + sizeof(rec_hdr))))
									{
										SECSHR_ACCOUNTING(7);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING(cs);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(cs->index);
										SECSHR_ACCOUNTING(cs->ins_off);
										SECSHR_ACCOUNTING(
											((blk_hdr *)blk_ptr)->bsiz);
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
								((blk_hdr *)blk_ptr)->tn = csd->trans_hist.curr_tn;
							}
							if (cs->first_off)
							{
								for (chain_ptr = blk_ptr + cs->first_off; ;
									chain_ptr += chain.next_off)
								{
									GET_LONGP(&chain, chain_ptr);
									if ((1 == chain.flag)
									   && ((chain_ptr - blk_ptr + sizeof(block_id))
										  <= ((blk_hdr *)blk_ptr)->bsiz)
									   && (chain.cw_index < si->cw_set_depth)
									   && (TRUE == secshr_tp_get_cw(
									      first_cw_set, chain.cw_index, &cs_ptr)))
									{
										PUT_LONG(chain_ptr, cs_ptr->blk);
										if (0 == chain.next_off)
											break;
									} else
									{
										SECSHR_ACCOUNTING(11);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING(cs);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(cs->index);
										SECSHR_ACCOUNTING(blk_ptr);
										SECSHR_ACCOUNTING(chain_ptr);
										SECSHR_ACCOUNTING(chain.next_off);
										SECSHR_ACCOUNTING(chain.cw_index);
										SECSHR_ACCOUNTING(si->cw_set_depth);
										SECSHR_ACCOUNTING(
											((blk_hdr *)blk_ptr)->bsiz);
										break;
									}
								}
							}
						}	/* TP */
					}	/* non-map processing */
					cs->mode = gds_t_committed;
				}	/* for all cw_set entries */
			}	/* if (NULL != first_cw_set) */
			if (JNL_ENABLED(csd))
			{
				if (GTM_PROBE(sizeof(jnl_private_control), csa->jnl, WRITE))
				{
					jbp = csa->jnl->jnl_buff;
					if (GTM_PROBE(sizeof(jnl_buffer), jbp, WRITE) && is_exiting)
					{
						CHECK_UNIX_LATCH(&jbp->fsync_in_prog_latch, is_exiting);
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
						if (csa->jnl->free_update_inprog)
						{
							jbp->free = csa->jnl->temp_free;
							jbp->freeaddr = csa->jnl->new_freeaddr;
						}
						if (jbp->blocked == rundown_process_id)
							jbp->blocked = 0;
					}
				} else
				{
					SECSHR_ACCOUNTING(4);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(csa->jnl);
					SECSHR_ACCOUNTING(sizeof(jnl_private_control));
				}
			}
			if (is_exiting && csa->freeze && csd->freeze == rundown_process_id && !csa->persistent_freeze)
			{
				csd->image_count = 0;
				csd->freeze = 0;
			}
			if (is_bg && (csa->wbuf_dqd || csa->now_crit))
			{	/* if csa->wbuf_dqd == TRUE, most likely failed during REMQHI in wcs_wtstart
				 * or db_csh_get.  cache corruption is suspected so set wc_blocked.
				 * if csa->now_crit is TRUE, someone else should clean the cache, so set wc_blocked.
				 */
				SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
				if (csa->now_crit)
				{
					wcblocked_ptr = WCBLOCKED_NOW_CRIT_LIT;
					BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_now_crit);
				} else
				{
					wcblocked_ptr = WCBLOCKED_WBUF_DQD_LIT;
					BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_wbuf_dqd);
				}
				UNIX_ONLY(
					/* cannot send oplog message in VMS as privileged routines cannot do I/O */
					send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_STR(wcblocked_ptr),
						rundown_process_id, &csd->trans_hist.curr_tn, DB_LEN_STR(reg));
				)
			}
			csa->wbuf_dqd = 0;	/* We can clear the flag now */
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
						INCREMENT_CURR_TN(csd);
					} else
						csd->trans_hist.early_tn = csd->trans_hist.curr_tn;
				}
				assert(csd->trans_hist.early_tn == csd->trans_hist.curr_tn);
				csa->t_commit_crit = FALSE;	/* ensure we don't process this region again */
				if ((GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE)) &&
					(GTM_PROBE(CRIT_SPACE, csa->critical, WRITE)))
				{
					if (csa->nl->in_crit == rundown_process_id)
						csa->nl->in_crit = 0;
					UNIX_ONLY(
						DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
						mutex_unlockw(reg, crash_count);
						DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
					)
					VMS_ONLY(
						mutex_stoprelw(csa->critical);
						csa->now_crit = FALSE;
					)
					UNSUPPORTED_PLATFORM_CHECK;
				} else
				{
					SECSHR_ACCOUNTING(6);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(csa->nl);
					SECSHR_ACCOUNTING(NODE_LOCAL_SIZE_DBS);
					SECSHR_ACCOUNTING(csa->critical);
					SECSHR_ACCOUNTING(CRIT_SPACE);
				}
			} else  if (csa->read_lock)
			{
				if (GTM_PROBE(CRIT_SPACE, csa->critical, WRITE))
				{
					VMS_ONLY(mutex_stoprelr(csa->critical);)
					csa->read_lock = FALSE;
				} else
				{
					SECSHR_ACCOUNTING(4);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(csa->critical);
					SECSHR_ACCOUNTING(CRIT_SPACE);
				}
			}
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
					if (COMPSWAP(&csa->shmpool_buffer->shmpool_crit_latch, pid, imgcnt,
						     rundown_process_id, rundown_image_count))
					{	/* Now our lock .. set blocked and release.  */
						csa->shmpool_buffer->shmpool_blocked = TRUE;
						BG_TRACE_PRO_ANY(csa, shmpool_blkd_by_sdc);
						DEBUG_LATCH(util_out_print("Orphaned latch cleaned up", TRUE));
						COMPSWAP(&csa->shmpool_buffer->shmpool_crit_latch, rundown_process_id,
							 rundown_image_count, LOCK_AVAILABLE, 0);
					} /* Else someone else took care of it */
				}
			}
#ifdef UNIX
			/* All releases done now. Double check latch is really cleared */
			if ((GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE)) &&
			    (GTM_PROBE(CRIT_SPACE, csa->critical, WRITE)))
			{
				CHECK_UNIX_LATCH(&csa->critical->semaphore, is_exiting);
				CHECK_UNIX_LATCH(&csa->critical->crashcnt_latch, is_exiting);
				CHECK_UNIX_LATCH(&csa->critical->prochead.latch, is_exiting);
				CHECK_UNIX_LATCH(&csa->critical->freehead.latch, is_exiting);
			}
#endif
		}	/* For all regions */
	}	/* For all glds */
	if (jnlpool_reg_addrs && (GTM_PROBE(sizeof(jnlpool_reg_addrs), jnlpool_reg_addrs, READ)))
	{	/* although there is only one jnlpool reg, SECSHR_PROBE_REGION macro might do a "continue" and hence the for loop */
		for (reg = *jnlpool_reg_addrs, jnlpool_reg = TRUE; jnlpool_reg && reg; jnlpool_reg = FALSE) /* only jnlpool reg */
		{
			SECSHR_PROBE_REGION(reg);	/* SECSHR_PROBE_REGION sets csa */
			if (csa->now_crit)
			{
				assert(NORMAL_TERMINATION != secshr_state); /* for normal termination we should not
									     * have been holding the journal pool crit lock */
				jpl = (jnlpool_ctl_ptr_t)((sm_uc_ptr_t)csa->critical - JNLPOOL_CTL_SIZE); /* see jnlpool_init() for
													   * relationship between
													   * critical and jpl */
				if (GTM_PROBE(sizeof(jnlpool_ctl_struct), jpl, WRITE))
				{
					if ((jpl->early_write_addr > jpl->write_addr) && (update_underway))
					{ /* we need to update journal pool to reflect the increase in jnl-seqno */
						cumul_jnl_rec_len = jpl->early_write_addr - jpl->write_addr;
						jh = (jnldata_hdr_ptr_t)((sm_uc_ptr_t)jpl + JNLDATA_BASE_OFF + jpl->write);
						if (GTM_PROBE(sizeof(*jh), jh, WRITE) && 0 != (jsize = jpl->jnlpool_size))
						{ /* Below chunk of code mirrors  what is done in t_end/tp_tend */
							/* Begin atomic stmnts */
							jh->jnldata_len = cumul_jnl_rec_len;
							jh->prev_jnldata_len = jpl->lastwrite_len;
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
				}
				if ((GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE)) &&
					(GTM_PROBE(CRIT_SPACE, csa->critical, WRITE)))
				{
					if (csa->nl->in_crit == rundown_process_id)
						csa->nl->in_crit = 0;
					UNIX_ONLY(
						DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
						mutex_unlockw(reg, 0);
						DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
					)
					VMS_ONLY(
						mutex_stoprelw(csa->critical);
						csa->now_crit = FALSE;
					)
					/* the above takes care of rolling forward step (11) of the commit flow */
				}
			}
		}
	}
	return;
}

bool secshr_tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1)
{
	int	iter;

	*cs1 = cs;
	for (iter = 0; iter < depth; iter++)
	{
		if (!(GTM_PROBE(sizeof(cw_set_element), *cs1, READ)))
		{
			*cs1 = NULL;
			return FALSE;
		}
		*cs1 = (*cs1)->next_cw_set;
	}
	if (*cs1 && GTM_PROBE(sizeof(cw_set_element), *cs1, READ))
	{
		while ((*cs1)->high_tlevel)
		{
			if (GTM_PROBE(sizeof(cw_set_element), (*cs1)->high_tlevel, READ))
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
