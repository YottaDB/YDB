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
#endif

#include "ast.h"	/* needed for JNL_ENSURE_OPEN_WCS_WTSTART macro in gdsfhead.h */
#include "copy.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "iosp.h"
#include "interlock.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "gdsbgtr.h"
#include "sleep_cnt.h"
#include "send_msg.h"
#include "t_qread.h"
#include "gvcst_blk_build.h"
#include "mm_read.h"
#include "is_proc_alive.h"
#include "cache.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "hashtab.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "wcs_sleep.h"
#include "add_inter.h"
#include "wbox_test_init.h"
#include "memcoherency.h"
#include "wcs_flu.h"		/* for SET_CACHE_FAIL_STATUS macro */
#ifdef UNIX
# ifdef GTM_CRYPT
#  include "gtmcrypt.h"
# endif
#include "io.h"			/* needed by gtmsecshr.h */
#include "gtmsecshr.h"		/* for continue_proc */
#endif
#include "wcs_phase2_commit_wait.h"
#include "gtm_c_stack_trace.h"

GBLDEF srch_blk_status	*first_tp_srch_status;	/* the first srch_blk_status for this block in this transaction */
GBLDEF unsigned char	rdfail_detail;	/* t_qread uses a 0 return to indicate a failure (no buffer filled) and the real
					status of the read is returned using a global reference, as the status detail
					should typically not be needed and optimizing the call is important */

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	short			crash_count;
GBLREF	uint4			dollar_tlevel;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			process_id;
GBLREF	boolean_t		tp_restart_syslog;	/* for the TP_TRACE_HIST_MOD macro */
GBLREF	gv_namehead		*gv_target;
GBLREF	boolean_t		dse_running;
GBLREF	boolean_t		disk_blk_read;
GBLREF	uint4			t_err;
GBLREF	boolean_t		block_is_free;
GBLREF	boolean_t		mupip_jnl_recover;

/* There are 3 passes (of the do-while loop below) we allow now.
 * The first pass which is potentially out-of-crit and hence can end up not locating the cache-record for the input block.
 * The second pass which holds crit and is waiting for a concurrent reader to finish reading the input block in.
 * The third pass is needed because the concurrent reader (in dsk_read) might encounter a DYNUPGRDFAIL error in which case
 *	it is going to increment the cycle in the cache-record and reset the blk to CR_BLKEMPTY.
 * We dont need any pass more than this because if we hold crit then no one else can start a dsk_read for this block.
 * This # of passes is hardcoded in the macro BAD_LUCK_ABOUNDS
 */
#define BAD_LUCK_ABOUNDS 2

#define	RESET_FIRST_TP_SRCH_STATUS(first_tp_srch_status, newcr, newcycle)				\
	assert((first_tp_srch_status)->cr != (newcr) || (first_tp_srch_status)->cycle != (newcycle));	\
	(first_tp_srch_status)->cr = (newcr);								\
	(first_tp_srch_status)->cycle = (newcycle);							\
	(first_tp_srch_status)->buffaddr = (sm_uc_ptr_t)GDS_REL2ABS((newcr)->buffaddr);

#define REL_CRIT_IF_NEEDED(CSA, REG, WAS_CRIT, HOLD_ONTO_CRIT)						\
{	/* If currently have crit, but didn't have it upon entering, release crit now. */		\
	assert(!WAS_CRIT || CSA->now_crit);								\
	if ((WAS_CRIT != CSA->now_crit) && !HOLD_ONTO_CRIT)						\
		rel_crit(REG);										\
}

error_def(ERR_BUFOWNERSTUCK);
error_def(ERR_CRYPTBADCONFIG);
error_def(ERR_DBFILERR);
error_def(ERR_DYNUPGRDFAIL);
error_def(ERR_GVPUTFAIL);

sm_uc_ptr_t t_qread(block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr_out)
	/* cycle is used in t_end to detect if the buffer has been refreshed since the t_qread */
{
	int4			status;
	uint4			blocking_pid;
	cache_rec_ptr_t		cr;
	bt_rec_ptr_t		bt;
	boolean_t		clustered, hold_onto_crit, was_crit;
	int			dummy, lcnt, ocnt;
	cw_set_element		*cse;
	off_chain		chain1;
	register sgmnt_addrs	*csa;
	register sgmnt_data_ptr_t	csd;
	enum db_ver		ondsk_blkver;
	int4			dummy_errno, gtmcrypt_errno;
	boolean_t		already_built, is_mm, reset_first_tp_srch_status, set_wc_blocked, sleep_invoked;
	ht_ent_int4		*tabent;
	srch_blk_status		*blkhist;
	trans_num		dirty, blkhdrtn;
	sm_uc_ptr_t		buffaddr;
	uint4			stuck_cnt = 0;
	boolean_t		lcl_blk_free;
	node_local_ptr_t	cnl;
#	ifdef GTM_CRYPT
	gd_segment		*seg;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	lcl_blk_free = block_is_free;
	block_is_free = FALSE;	/* Reset to FALSE so that if t_qread fails below, we don't have an incorrect state of this var */
	first_tp_srch_status = NULL;
	reset_first_tp_srch_status = FALSE;
	csa = cs_addrs;
	csd = csa->hdr;
	INCR_DB_CSH_COUNTER(csa, n_t_qreads, 1);
	is_mm = (dba_mm == csd->acc_meth);
	/* We better hold crit in the final retry (TP & non-TP). Only exception is journal recovery */
	assert((t_tries < CDB_STAGNATE) || csa->now_crit || mupip_jnl_recover);
	if (dollar_tlevel)
	{
		assert(sgm_info_ptr);
		if (0 != sgm_info_ptr->cw_set_depth)
		{
			chain1 = *(off_chain *)&blk;
			if (1 == chain1.flag)
			{
				assert(sgm_info_ptr->cw_set_depth);
				if ((int)chain1.cw_index < sgm_info_ptr->cw_set_depth)
					tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain1.cw_index, &cse);
				else
				{
					assert(FALSE == csa->now_crit);
					rdfail_detail = cdb_sc_blknumerr;
					return (sm_uc_ptr_t)NULL;
				}
			} else
			{
				if (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use, (uint4 *)&blk)))
					first_tp_srch_status = tabent->value;
				else
					first_tp_srch_status = NULL;
				ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(first_tp_srch_status, sgm_info_ptr);
				cse = first_tp_srch_status ? first_tp_srch_status->cse : NULL;
			}
			assert(!cse || !cse->high_tlevel);
			assert(!chain1.flag || cse);
			if (cse)
			{	/* transaction has modified the sought after block  */
				if ((gds_t_committed != cse->mode) || (n_gds_t_op < cse->old_mode))
				{	/* Changes have not been committed to shared memory, i.e. still in private memory.
					 * Build block in private buffer if not already done and return the same.
					 */
					assert(gds_t_writemap != cse->mode);
					if (FALSE == cse->done)
					{	/* out of date, so make it current */
						assert(gds_t_committed != cse->mode);
						already_built = (NULL != cse->new_buff);
						/* Validate the block's search history right after building a private copy.
						 * This is not needed in case gvcst_search is going to reuse the clue's search
						 * history and return (because tp_hist will do the validation of this block).
						 * But if gvcst_search decides to do a fresh traversal (because the clue does not
						 * cover the path of the current input key etc.) the block build that happened now
						 * will not get validated in tp_hist since it will instead be given the current
						 * key's search history path (a totally new path) for validation. Since a private
						 * copy of the block has been built, tp_tend would also skip validating this block
						 * so it is necessary that we validate the block right here. Since it is tricky to
						 * accurately differentiate between the two cases, we do the validation
						 * unconditionally here (besides it is only a few if checks done per block build
						 * so it is considered okay performance-wise).
						 */
						gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, 0);
						assert(NULL != cse->blk_target);
						if (!already_built && !chain1.flag)
						{
							buffaddr = first_tp_srch_status->buffaddr;
							cr = first_tp_srch_status->cr;
							assert((is_mm || cr) && buffaddr);
							blkhdrtn = ((blk_hdr_ptr_t)buffaddr)->tn;
							if (TP_IS_CDB_SC_BLKMOD3(cr, first_tp_srch_status, blkhdrtn))
							{
								assert(CDB_STAGNATE > t_tries);
								rdfail_detail = cdb_sc_blkmod;	/* should this be something else */
								TP_TRACE_HIST_MOD(blk, gv_target, tp_blkmod_t_qread, cs_data,
									first_tp_srch_status->tn, blkhdrtn,
									((blk_hdr_ptr_t)buffaddr)->levl);
								return (sm_uc_ptr_t)NULL;
							}
							if (!is_mm && ((first_tp_srch_status->cycle != cr->cycle)
										|| (first_tp_srch_status->blk_num != cr->blk)))
							{
								assert(CDB_STAGNATE > t_tries);
								rdfail_detail = cdb_sc_lostcr; /* should this be something else */
								return (sm_uc_ptr_t)NULL;
							}
						}
						cse->done = TRUE;
					}
					*cycle = CYCLE_PVT_COPY;
					*cr_out = 0;
					return (sm_uc_ptr_t)cse->new_buff;
				} else
				{	/* Block changes are already committed to shared memory (possible if we are in TP
					 * in the 2nd phase of M-Kill in gvcst_expand_free_subtree.c). In this case, read
					 * block from shared memory; do not look at private memory (i.e. cse) as that might
					 * not be as uptodate as shared memory.
					 */
					assert(csa->now_crit);	/* gvcst_expand_free_subtree does t_qread in crit */
					/* If this block was newly created as part of the TP transaction, it should not be killed
					 * as part of the 2nd phase of M-kill. This is because otherwise the block's cse would
					 * have had an old_mode of kill_t_create in which case we would not have come into this
					 * else block. Assert accordingly.
					 */
					assert(!chain1.flag);
					first_tp_srch_status = NULL;	/* do not use any previous srch_hist information */
				}
			}
		} else
		{
			if (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use, (uint4 *)&blk)))
				first_tp_srch_status = tabent->value;
			else
				first_tp_srch_status = NULL;
		}
		ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(first_tp_srch_status, sgm_info_ptr);
		if (!is_mm && first_tp_srch_status)
		{
			cr = first_tp_srch_status->cr;
			assert(cr && !first_tp_srch_status->cse);
			if (first_tp_srch_status->cycle == cr->cycle)
			{
				*cycle = first_tp_srch_status->cycle;
				*cr_out = cr;
				cr->refer = TRUE;
				if (CDB_STAGNATE <= t_tries)	/* mu_reorg doesn't use TP else should have an || for that */
					CWS_INSERT(blk);
				return (sm_uc_ptr_t)first_tp_srch_status->buffaddr;
			} else
			{	/* Block was already part of the read-set of this transaction, but got recycled in the cache.
				 * Allow block recycling by resetting first_tp_srch_status for this blk to reflect the new
				 * buffer, cycle and cache-record. tp_hist (invoked much later) has validation checks to detect
				 * if block recycling happened within the same mini-action and restart in that case.
				 * Updating first_tp_srch_status has to wait until the end of t_qread since only then do we know
				 * the values to update to. Set a variable that will enable the updation before returning.
				 * Also assert that if we are in the final retry, we are never in a situation where we have a
				 * block that got recycled since the start of the current mini-action. This is easily detected since
				 * as part of the final retry we maintain a hash-table "cw_stagnate" that holds the blocks that
				 * have been read as part of the current mini-action until now.
				 */
				assert(CDB_STAGNATE > t_tries || (NULL == lookup_hashtab_int4(&cw_stagnate, (uint4 *)&blk)));
				reset_first_tp_srch_status = TRUE;
			}
		}
	}
	if ((uint4)blk >= (uint4)csa->ti->total_blks)
	{	/* Requested block out of range; could occur because of a concurrency conflict. mm_read and dsk_read assume blk is
		 * never negative or greater than the maximum possible file size. If a concurrent REORG truncates the file, t_qread
		 * can proceed despite blk being greater than total_blks. But dsk_read handles this fine; see comments below.
		 */
		assert((&FILE_INFO(gv_cur_region)->s_addrs == csa) && (csd == cs_data));
		assert(!csa->now_crit);
		rdfail_detail = cdb_sc_blknumerr;
		return (sm_uc_ptr_t)NULL;
	}
	if (is_mm)
	{
		*cycle = CYCLE_SHRD_COPY;
		*cr_out = 0;
		return (sm_uc_ptr_t)(mm_read(blk));
	}
#	ifdef GTM_CRYPT
	if ((GTMCRYPT_INVALID_KEY_HANDLE == csa->encr_key_handle) && !IS_BITMAP_BLK(blk))
	{	/* A non-GT.M process is attempting to read a non-bitmap block but doesn't have a valid encryption key handle. This
		 * is an indication that the process encountered an error during db_init and reported it with a -W- severity. But,
		 * since the block it is attempting to read can be in the unencrypted shared memory, we cannot let it access it
		 * without a valid handle. So, issue an rts_error
		 */
		assert(!IS_GTM_IMAGE);	/* GT.M would have error'ed out in db_init */
		gtmcrypt_errno = SET_REPEAT_MSG_MASK(SET_CRYPTERR_MASK(ERR_CRYPTBADCONFIG));
		seg = gv_cur_region->dyn.addr;
		GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
	}
#	endif
	assert(dba_bg == csd->acc_meth);
	assert(!first_tp_srch_status || !first_tp_srch_status->cr
					|| first_tp_srch_status->cycle != first_tp_srch_status->cr->cycle);
	if (FALSE == (clustered = csd->clustered))
		bt = NULL;
	was_crit = csa->now_crit;
	ocnt = 0;
	cnl = csa->nl;
	set_wc_blocked = FALSE;	/* to indicate whether cnl->wc_blocked was set to TRUE by us */
	hold_onto_crit = csa->hold_onto_crit;	/* note down in local to avoid csa-> dereference in multiple usages below */
	do
	{
		if (NULL == (cr = db_csh_get(blk)))
		{	/* not in memory */
			if (clustered && (NULL != (bt = bt_get(blk))) && (FALSE == bt->flushing))
				bt = NULL;
			if (!csa->now_crit)
			{
				assert(!hold_onto_crit);
				if (NULL != bt)
				{	/* at this point, bt is not NULL only if clustered and flushing - wait no crit */
					assert(clustered);
					wait_for_block_flush(bt, blk);	/* try for no other node currently writing the block */
				}
				if ((csd->flush_trigger <= cnl->wcs_active_lvl) && (FALSE == gv_cur_region->read_only))
					JNL_ENSURE_OPEN_WCS_WTSTART(csa, gv_cur_region, 0, dummy_errno);
						/* a macro that dclast's "wcs_wtstart" and checks for errors etc. */
				grab_crit(gv_cur_region);
				cr = db_csh_get(blk);			/* in case blk arrived before crit */
			}
			if (clustered && (NULL != (bt = bt_get(blk))) && (TRUE == bt->flushing))
			{	/* Once crit, need to assure that if clustered, that flushing is [still] complete
				 * If it isn't, we missed an entire WM cycle and have to wait for another node to finish */
				wait_for_block_flush(bt, blk);	/* ensure no other node currently writing the block */
			}
			if (NULL == cr)
			{	/* really not in memory - must get a new buffer */
				assert(csa->now_crit);
				cr = db_csh_getn(blk);
				if (CR_NOTVALID == (sm_long_t)cr)
				{
					assert(cnl->wc_blocked); /* only reason we currently know wcs_get_space could fail */
					assert(gtm_white_box_test_case_enabled);
					SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_db_csh_getn_invalid_blk);
					set_wc_blocked = TRUE;
					break;
				}
				assert(0 <= cr->read_in_progress);
				*cycle = cr->cycle;
				cr->tn = csd->trans_hist.curr_tn;
				/* Record history of most recent disk reads only in dbg builds for now. Although the macro
				 * is just a couple dozen instructions, it is done while holding crit so we want to avoid
				 * delaying crit unless really necessary. Whoever wants this information can enable it
				 * by a build change to remove the DEBUG_ONLY part below.
				 */
				DEBUG_ONLY(DSKREAD_TRACE(csa, GDS_ANY_ABS2REL(csa,cr), cr->tn, process_id, blk, cr->cycle);)
				if (!was_crit && !hold_onto_crit)
					rel_crit(gv_cur_region);
				/* read outside of crit may be of a stale block but should be detected by t_end or tp_tend */
				assert(0 == cr->dirty);
				assert(cr->read_in_progress >= 0);
				CR_BUFFER_CHECK(gv_cur_region, csa, csd, cr);
				buffaddr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
				if (SS_NORMAL != (status = dsk_read(blk, buffaddr, &ondsk_blkver, lcl_blk_free)))
				{	/* buffer does not contain valid data, so reset blk to be empty */
					cr->cycle++;	/* increment cycle for blk number changes (for tp_hist and others) */
					cr->blk = CR_BLKEMPTY;
					cr->r_epid = 0;
					RELEASE_BUFF_READ_LOCK(cr);
					assert(-1 <= cr->read_in_progress);
					assert(was_crit == csa->now_crit);
					if (FUTURE_READ == status)
					{	/* in cluster, block can be in the "future" with respect to the local history */
						assert(TRUE == clustered);
						assert(FALSE == csa->now_crit);
						rdfail_detail = cdb_sc_future_read;	/* t_retry forces the history up to date */
						return (sm_uc_ptr_t)NULL;
					}
					if (ERR_DYNUPGRDFAIL == status)
					{	/* if we dont hold crit on the region, it is possible due to concurrency conflicts
						 * that this block is unused (i.e. marked free/recycled in bitmap, see comments in
						 * gds_blk_upgrade.h). in this case we should not error out but instead restart.
						 */
						if (was_crit)
						{
							assert(FALSE);
							rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) status, 3, blk,
									DB_LEN_STR(gv_cur_region));
						} else
						{
							rdfail_detail = cdb_sc_lostcr;
							return (sm_uc_ptr_t)NULL;
						}
					}
					if ((-1 == status) && !was_crit)
					{	/* LSEEKREAD and, consequently, dsk_read return -1 in case pread is unable to fetch
						 * a full database block's length of data. This can happen if the requested read is
						 * past the end of the file, which can happen if a concurrent truncate occurred
						 * after the blk >= csa->ti->total_blks comparison above. Allow for this scenario
						 * by restarting. However, if we've had crit the whole time, no truncate could have
						 * happened. -1 indicates a problem with the file, so fall through to DBFILERR.
						 */
						rdfail_detail = cdb_sc_truncate;
						return (sm_uc_ptr_t)NULL;
					}
#					ifdef GTM_CRYPT
					else if (IS_CRYPTERR_MASK(status))
					{
						seg = gv_cur_region->dyn.addr;
						GTMCRYPT_REPORT_ERROR(status, rts_error, seg->fname_len, seg->fname);
					}
#					endif
					else
					{	/* A DBFILERR can be thrown for two possible reasons:
						 * (1) LSEEKREAD returned an unexpected error due to a filesystem problem; or
						 * (2) csa/cs_addrs/csd/cs_data are out of sync, and we're trying to read a block
						 * number for one region from another region with fewer total_blks.
						 *    We suspect the former is what happened in GTM-7623. Apparently the latter
						 * has been an issue before, too. If either occurs again in pro, this assertpro
						 * distinguishes the two possibilities.
						 */
						assertpro((&FILE_INFO(gv_cur_region)->s_addrs == csa) && (csd == cs_data));
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region),
								status);
					}
				}
				disk_blk_read = TRUE;
				assert(0 <= cr->read_in_progress);
				assert(0 == cr->dirty);
				/* Only set in cache if read was success */
				cr->ondsk_blkver = (lcl_blk_free ? GDSVCURR : ondsk_blkver);
				cr->r_epid = 0;
				RELEASE_BUFF_READ_LOCK(cr);
				assert(-1 <= cr->read_in_progress);
				*cr_out = cr;
				assert(was_crit == csa->now_crit);
				if (reset_first_tp_srch_status)
				{	/* keep the parantheses for the if (although single line) since the following is a macro */
					RESET_FIRST_TP_SRCH_STATUS(first_tp_srch_status, cr, *cycle);
				}
				return buffaddr;
			} else  if (!was_crit && (BAD_LUCK_ABOUNDS > ocnt))
			{
				assert(!hold_onto_crit);
				assert(TRUE == csa->now_crit);
				assert(cnl->in_crit == process_id);
				rel_crit(gv_cur_region);
			}
		}
		if (CR_NOTVALID == (sm_long_t)cr)
		{
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_db_csh_get_invalid_blk);
			set_wc_blocked = TRUE;
			break;
		}
		/* It is very important for cycle to be noted down BEFORE checking for read_in_progress/in_tend.
		 * Because of this instruction order requirement, we need to have a read barrier just after noting down cr->cycle.
		 * Doing it the other way round introduces the scope for a bug in the concurrency control validation logic in
		 * t_end/tp_hist/tp_tend. This is because the validation logic relies on t_qread returning an atomically
		 * consistent value of <"cycle","cr"> for a given input blk such that cr->buffaddr held the input blk's
		 * contents at the time when cr->cycle was "cycle". It is important that cr->read_in_progress is -1
		 * (indicating the read from disk into the buffer is complete) AND cr->in_tend is FALSE (indicating
		 * that the buffer is not being updated) when t_qread returns. The only exception is if cr->cycle is higher
		 * than the "cycle" returned by t_qread (signifying the buffer got reused for another block concurrently)
		 * in which case the cycle check in the validation logic will detect this.
		 */
		*cycle = cr->cycle;
		SHM_READ_MEMORY_BARRIER;
		sleep_invoked = FALSE;
		for (lcnt = 1;  ; lcnt++)
		{
			if (0 > cr->read_in_progress)
			{	/* it's not being read */
				if (clustered && (0 == cr->bt_index) && (cr->tn < OLDEST_HIST_TN(csa)))
				{	/* can't rely on the buffer */
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					break;
				}
				*cr_out = cr;
				VMS_ONLY(
					/* If we were doing the "db_csh_get" above (in t_qread itself) and located the cache-record
					 * which, before coming here and taking a copy of cr->cycle a few lines above, was made an
					 * older twin by another process in bg_update (note this can happen in VMS only) which has
					 * already incremented the cycle, we will end up having a copy of the old cache-record with
					 * its incremented cycle number and hence will succeed in tp_hist validation if we return
					 * this <cr,cycle> combination although we don't want to since this "cr" is not current for
					 * the given block as of now. Note that the "indexmod" optimization in "tp_tend" relies on
					 * an accurate intermediate validation by "tp_hist" which in turn relies on the <cr,cycle>
					 * value returned by t_qread to be accurate for a given blk at the current point in time.
					 * We detect the older-twin case by the following check. Note that here we depend on the
					 * the fact that "bg_update" sets cr->bt_index to 0 before incrementing cr->cycle.
					 * Given that order, cr->bt_index can be guaranteed to be 0 if we read the incremented cycle
					 */
					if (cr->twin && (0 == cr->bt_index))
						break;
				)
				if (cr->blk != blk)
					break;
				REL_CRIT_IF_NEEDED(csa, gv_cur_region, was_crit, hold_onto_crit);
				assert(was_crit == csa->now_crit);
				/* Check if "cr" is locked for phase2 update by a concurrent process. Before doing so, need to
				 * do a read memory barrier to ensure we read a consistent state. Otherwise, we could see
				 * cr->in_tend as 0 even though it is actually non-zero in another processor (due to cache
				 * coherency delays in multi-processor environments) and this could lead to mysterious
				 * failures including GTMASSERTs and database damage as the validation logic in t_end/tp_tend
				 * relies on the fact that the cr->in_tend check here is accurate as of this point.
				 *
				 * Note that on architectures where a change done by another process needs two steps to be made
				 * visible by another process (write memory barrier on the writer side AND a read memory barrier
				 * on the reader side) this read memory barrier also serves the purpose of ensuring this process
				 * sees an uptodate state of the global buffer whose contents got modified by the disk read (done
				 * by another process) that finished just now. Example is the Alpha architecture where this is
				 * needed. Example where this is not needed is the Power architecture (as of this writing) where
				 * only the write memory barrier on the write side is necessary. As long as the reader sees any
				 * update done AFTER the write memory barrier, it is guaranteed to see all updates done BEFORE
				 * the write memory barrier.
				 */
				SHM_READ_MEMORY_BARRIER;
				blocking_pid = cr->in_tend;
				if (blocking_pid)
				{	/* Wait for cr->in_tend to be non-zero. But in the case we are doing a TP transaction and
					 * the global has NOISOLATION turned ON and this is a leaf level block and this is a SET
					 * operation (t_err == ERR_GVPUTFAIL), avoid the sleep but ensure a cdb_sc_blkmod type
					 * restart will be triggered (in tp_tend) and the function "recompute_upd_array" will be
					 * invoked. Avoiding the sleep in this case (at the cost of recomputing the update array
					 * in crit) is expected to improve throughput. The only exception is if we are in the
					 * final retry in which case it is better to wait here as we dont want to end up in a
					 * situation where "recompute_upd_array" indicates that a restart is necessary.
					 */
					if (dollar_tlevel && (gv_target && gv_target->noisolation) && (ERR_GVPUTFAIL == t_err)
						&& (CDB_STAGNATE > t_tries))	/* do not skip wait in case of final retry */
					{	/* We know that the only caller in this case would be the function "gvcst_search".
						 * If the input cr and cycle match corresponding fields of gv_target->hist.h[0],
						 * we update the corresponding "tn" field to reset it BACK thereby ensuring the
						 * cdb_sc_blkmod check in tp_tend will fail and that the function
						 * "recompute_upd_array" will be invoked to try and recompute the update array.
						 * We do this only in case of gv_target->hist.h[0] as recomputations
						 * are currently done for NOISOLATION globals only for leaf level blocks.
						 */
						blkhist = &gv_target->hist.h[0];
						dirty = cr->dirty;
						if (((sm_int_ptr_t)&blkhist->cycle == (sm_int_ptr_t)cycle)
							&& ((cache_rec_ptr_ptr_t)&blkhist->cr == (cache_rec_ptr_ptr_t)cr_out))
						{
							if (blkhist->tn > dirty)
							{
								blkhist->tn = dirty;
								if (reset_first_tp_srch_status)
									first_tp_srch_status->tn = dirty;
							}
							blocking_pid = 0;	/* do not sleep in the for loop below */
						}
					}
					if (blocking_pid)
					{
						if (TREF(tqread_nowait) && ((sm_int_ptr_t)&gv_target->hist.h[0].cycle == cycle))
						{	/* We're an update helper. Don't waste time waiting on a leaf blk */
							rdfail_detail = cdb_sc_tqreadnowait;
							return (sm_uc_ptr_t)NULL;
						}
						if (!wcs_phase2_commit_wait(csa, cr))
						{	/* Timed out waiting for cr->in_tend to become non-zero. Restart. */
							rdfail_detail = cdb_sc_phase2waitfail;
							return (sm_uc_ptr_t)NULL;
						}
					}
				}
				if (reset_first_tp_srch_status)
				{	/* keep the parantheses for the if (although single line) since the following is a macro */
					RESET_FIRST_TP_SRCH_STATUS(first_tp_srch_status, cr, *cycle);
				}
				assert(!csa->now_crit || !cr->twin || cr->bt_index);
				assert(!csa->now_crit || (NULL == (bt = bt_get(blk)))
					|| (CR_NOTVALID == bt->cache_index)
					|| (cr == (cache_rec_ptr_t)GDS_REL2ABS(bt->cache_index)) && (0 == cr->in_tend));
				/* Note that at this point we expect t_qread to return a <cr,cycle> combination that
				 * corresponds to "blk" passed in. It is crucial to get an accurate value for both the fields
				 * since "tp_hist" relies on this for its intermediate validation.
				 */
				return (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
			}
			if (blk != cr->blk)
				break;
			if (lcnt >= BUF_OWNER_STUCK && (0 == (lcnt % BUF_OWNER_STUCK)))
			{
				if (!csa->now_crit && !hold_onto_crit)
					grab_crit(gv_cur_region);
				if (cr->read_in_progress < -1)
				{	/* outside of design; clear to known state */
					BG_TRACE_PRO(t_qread_out_of_design);
					assert(0 == cr->r_epid);
					cr->r_epid = 0;
					INTERLOCK_INIT(cr);
				} else if (cr->read_in_progress >= 0)
				{
					BG_TRACE_PRO(t_qread_buf_owner_stuck);
					blocking_pid = cr->r_epid;
					if ((0 != blocking_pid) && (process_id != blocking_pid))
					{
						if (FALSE == is_proc_alive(blocking_pid, cr->image_count))
						{	/* process gone: release that process's lock */
							assert(0 == cr->bt_index);
							if (cr->bt_index)
							{
								SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_bad_bt_index1);
								set_wc_blocked = TRUE;
								break;
							}
							cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
							cr->blk = CR_BLKEMPTY;
							cr->r_epid = 0;
							RELEASE_BUFF_READ_LOCK(cr);
						} else
						{
							if (!hold_onto_crit)
								rel_crit(gv_cur_region);
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2,
									DB_LEN_STR(gv_cur_region));
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_BUFOWNERSTUCK, 7, process_id,
									blocking_pid, cr->blk, cr->blk, (lcnt / BUF_OWNER_STUCK),
								 	cr->read_in_progress, cr->rip_latch.u.parts.latch_pid);
							stuck_cnt++;
							GET_C_STACK_FROM_SCRIPT("BUFOWNERSTUCK", process_id, blocking_pid,
										stuck_cnt);
							/* Kickstart the process taking a long time in case it was suspended */
							UNIX_ONLY(continue_proc(blocking_pid));
						}
					} else
					{	/* process stopped before could set r_epid OR
						 * Process is waiting on the lock held by itself.
						 * Process waiting on the lock held by itself is an out-of-design
						 * situation that we dont how it can occur hence the following assert
						 * but know how to handle so we dont have to gtmassert in pro.
						 */
						assert(process_id != blocking_pid);
						assert(0 == cr->bt_index);
						if (cr->bt_index)
						{
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_bad_bt_index2);
							set_wc_blocked = TRUE;
							break;
						}
						cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
						cr->blk = CR_BLKEMPTY;
						cr->r_epid = 0; /* If the process itself is lock holder, r_epid is non-zero */
						RELEASE_BUFF_READ_LOCK(cr);
						if (cr->read_in_progress < -1)	/* race: process released since if r_epid */
							LOCK_BUFF_FOR_READ(cr, dummy);
					}
				}
				REL_CRIT_IF_NEEDED(csa, gv_cur_region, was_crit, hold_onto_crit);
			} else
			{
				if (TREF(tqread_nowait) && ((sm_int_ptr_t)&gv_target->hist.h[0].cycle == cycle))
				{	/* We're an update helper. Don't waste time waiting on a leaf blk; move on to useful work */
					REL_CRIT_IF_NEEDED(csa, gv_cur_region, was_crit, hold_onto_crit);
					rdfail_detail = cdb_sc_tqreadnowait;
					return (sm_uc_ptr_t)NULL;
				}
				BG_TRACE_PRO_ANY(csa, t_qread_ripsleep_cnt);
				if (!sleep_invoked)	/* Count # of blks for which we ended up sleeping on the read */
					BG_TRACE_PRO_ANY(csa, t_qread_ripsleep_nblks);
				wcs_sleep(lcnt);
				sleep_invoked = TRUE;
			}
		}
		if (set_wc_blocked)	/* cannot use cnl->wc_blocked here as we might not necessarily have crit */
			break;
		ocnt++;
		assert((0 == was_crit) || (1 == was_crit));
		/* if we held crit while entering t_qread we might need BAD_LUCK_ABOUNDS - 1 passes.
		 * otherwise we might need BAD_LUCK_ABOUNDS passes. if we are beyond this GTMASSERT.
		 */
		if ((BAD_LUCK_ABOUNDS - was_crit) < ocnt)
		{
			assert(!hold_onto_crit);
			assert(!csa->now_crit);
			GTMASSERT;
		}
		if (!csa->now_crit && !hold_onto_crit)
			grab_crit(gv_cur_region);
	} while (TRUE);
	assert(set_wc_blocked && (cnl->wc_blocked || !csa->now_crit));
	SET_CACHE_FAIL_STATUS(rdfail_detail, csd);
	REL_CRIT_IF_NEEDED(csa, gv_cur_region, was_crit, hold_onto_crit);
	assert(was_crit == csa->now_crit);
	return (sm_uc_ptr_t)NULL;
}
