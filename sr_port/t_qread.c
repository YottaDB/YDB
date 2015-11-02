/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
#include "cws_insert.h"
#include "wcs_sleep.h"
#include "add_inter.h"
#ifdef UNIX
#include "io.h"			/* needed by gtmsecshr.h */
#include "gtmsecshr.h"		/* for continue_proc */
#endif
#include "cert_blk.h"
#include "hashtab.h"

GBLDEF srch_blk_status	*first_tp_srch_status;	/* the first srch_blk_status for this block in this transaction */
GBLDEF unsigned char	rdfail_detail;	/* t_qread uses a 0 return to indicate a failure (no buffer filled) and the real
					status of the read is returned using a global reference, as the status detail
					should typically not be needed and optimizing the call is important */

GBLREF	bool			certify_all_blocks;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	short			crash_count;
GBLREF	short			dollar_tlevel;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			process_id;
GBLREF	boolean_t		tp_restart_syslog;	/* for the TP_TRACE_HIST_MOD macro */
GBLREF	gv_namehead		*gv_target;
GBLREF	boolean_t		dse_running;
GBLREF	boolean_t		disk_blk_read;

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

sm_uc_ptr_t t_qread(block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr_out)
	/* cycle is used in t_end to detect if the buffer has been refreshed since the t_qread */
{
	uint4			status, blocking_pid;
	cache_rec_ptr_t		cr;
	bt_rec_ptr_t		bt;
	bool			clustered, was_crit;
	int			dummy, lcnt, ocnt;
	cw_set_element		*cse;
	off_chain		chain1;
	register sgmnt_addrs	*csa;
	register sgmnt_data_ptr_t	csd;
	enum db_ver		ondsk_blkver;
	int4			dummy_errno;
	boolean_t		already_built, is_mm, reset_first_tp_srch_status, set_wc_blocked;
	ht_ent_int4		*tabent;

	error_def(ERR_BUFOWNERSTUCK);
	error_def(ERR_DBFILERR);
	error_def(ERR_DYNUPGRDFAIL);

	first_tp_srch_status = NULL;
	reset_first_tp_srch_status = FALSE;
	csa = cs_addrs;
	csd = csa->hdr;
	INCR_DB_CSH_COUNTER(csa, n_t_qreads, 1);
	is_mm = (dba_mm == csd->acc_meth);
	assert((t_tries < CDB_STAGNATE) || csa->now_crit);
	if (0 < dollar_tlevel)
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
				cse = first_tp_srch_status ? first_tp_srch_status->ptr : NULL;
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
						gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, 0);
						assert(NULL != cse->blk_target);
						if (!already_built && !chain1.flag)
						{
							assert(first_tp_srch_status && (is_mm || first_tp_srch_status->cr)
											&& first_tp_srch_status->buffaddr);
							if (first_tp_srch_status->tn <=
									((blk_hdr_ptr_t)(first_tp_srch_status->buffaddr))->tn)
							{
								assert(CDB_STAGNATE > t_tries);
								rdfail_detail = cdb_sc_blkmod;	/* should this be something else */
								TP_TRACE_HIST_MOD(blk, gv_target, tp_blkmod_t_qread, cs_data,
									first_tp_srch_status->tn,
									((blk_hdr_ptr_t)(first_tp_srch_status->buffaddr))->tn,
									((blk_hdr_ptr_t)(first_tp_srch_status->buffaddr))->levl);
								return (sm_uc_ptr_t)NULL;
							}
							if ((!is_mm)
								&& (first_tp_srch_status->cycle != first_tp_srch_status->cr->cycle
								|| first_tp_srch_status->blk_num != first_tp_srch_status->cr->blk))
							{
								assert(CDB_STAGNATE > t_tries);
								rdfail_detail = cdb_sc_lostcr;	/* should this be something else */
								return (sm_uc_ptr_t)NULL;
							}
							if (certify_all_blocks) /* will GTMASSERT on integ error */
								cert_blk(gv_cur_region, blk, (blk_hdr_ptr_t)cse->new_buff,
									cse->blk_target->root, TRUE);
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
			assert(first_tp_srch_status->cr && !first_tp_srch_status->ptr);
			if (first_tp_srch_status->cycle == first_tp_srch_status->cr->cycle)
			{
				*cycle = first_tp_srch_status->cycle;
				*cr_out = first_tp_srch_status->cr;
				first_tp_srch_status->cr->refer = TRUE;
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
	if ((blk >= csa->ti->total_blks) || (blk < 0))
	{	/* requested block out of range; could occur because of a concurrency conflict */
		if ((&FILE_INFO(gv_cur_region)->s_addrs != csa) || (csd != cs_data))
			GTMASSERT;
		assert(FALSE == csa->now_crit);
		rdfail_detail = cdb_sc_blknumerr;
		return (sm_uc_ptr_t)NULL;
	}
	if (is_mm)
	{
		*cycle = CYCLE_SHRD_COPY;
		*cr_out = 0;
		return (sm_uc_ptr_t)(mm_read(blk));
	}
	assert(dba_bg == csd->acc_meth);
	assert(!first_tp_srch_status || !first_tp_srch_status->cr
					|| first_tp_srch_status->cycle != first_tp_srch_status->cr->cycle);
	if (FALSE == (clustered = csd->clustered))
		bt = NULL;
	was_crit = csa->now_crit;
	ocnt = 0;
	set_wc_blocked = FALSE;	/* to indicate whether csd->wc_blocked was set to TRUE by us */
	do
	{
		if (NULL == (cr = db_csh_get(blk)))
		{	/* not in memory */
			if (clustered && (NULL != (bt = bt_get(blk))) && (FALSE == bt->flushing))
				bt = NULL;
			if (FALSE == csa->now_crit)
			{
				if (NULL != bt)
				{	/* at this point, bt is not NULL only if clustered and flushing - wait no crit */
					assert(clustered);
					wait_for_block_flush(bt, blk);	/* try for no other node currently writing the block */
				}
				if (csd->flush_trigger <= csa->nl->wcs_active_lvl  &&  FALSE == gv_cur_region->read_only)
					JNL_ENSURE_OPEN_WCS_WTSTART(csa, gv_cur_region, 0, dummy_errno);
						/* a macro that dclast's wcs_wtstart() and checks for errors etc. */
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
					SET_TRACEABLE_VAR(cs_data->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_db_csh_getn_invalid_blk);
					set_wc_blocked = TRUE;
					break;
				}
				assert(0 <= cr->read_in_progress);
				*cycle = cr->cycle;
				cr->tn = csa->ti->curr_tn;
				if (FALSE == was_crit)
					rel_crit(gv_cur_region);
				/* read outside of crit may be of a stale block but should be detected by t_end or tp_tend */
				assert(0 == cr->dirty);
				assert(cr->read_in_progress >= 0);
				INCR_DB_CSH_COUNTER(csa, n_dsk_reads, 1);
				CR_BUFFER_CHECK(gv_cur_region, csa, csd, cr);
				if (SS_NORMAL != (status = dsk_read(blk, GDS_REL2ABS(cr->buffaddr), &ondsk_blkver)))
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
							rts_error(VARLSTCNT(5) status, 3, blk, DB_LEN_STR(gv_cur_region));
						} else
						{
							rdfail_detail = cdb_sc_lostcr;
							return (sm_uc_ptr_t)NULL;
						}
					} else
						rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), status);
				}
				disk_blk_read = TRUE;
				assert(0 <= cr->read_in_progress);
				assert(0 == cr->dirty);
				cr->ondsk_blkver = ondsk_blkver;			/* Only set in cache if read was success */
				cr->r_epid = 0;
				RELEASE_BUFF_READ_LOCK(cr);
				assert(-1 <= cr->read_in_progress);
				*cr_out = cr;
				assert(was_crit == csa->now_crit);
				if (reset_first_tp_srch_status)
				{	/* keep the parantheses for the if (although single line) since the following is a macro */
					RESET_FIRST_TP_SRCH_STATUS(first_tp_srch_status, cr, *cycle);
				}
				return (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
			} else  if ((FALSE == was_crit) && (BAD_LUCK_ABOUNDS > ocnt))
			{
				assert(TRUE == csa->now_crit);
				assert(csa->nl->in_crit == process_id);
				rel_crit(gv_cur_region);
			}
		}
		if (CR_NOTVALID == (sm_long_t)cr)
		{
			SET_TRACEABLE_VAR(cs_data->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_db_csh_get_invalid_blk);
			set_wc_blocked = TRUE;
			break;
		}
		/* it is very important for cycle to be noted down before checking for read_in_progress. doing it
		 * the other way round introduces the scope for a bug in the concurrency control validation logic in
		 * t_end/tp_hist/tp_tend. this is because the validation logic relies on t_qread returning an atomically
		 * consistent value of <"cycle","cr"> for a given input blk such that cr->buffaddr held the input blk's
		 * contents at the time when cr->cycle was "cycle". it is important that cr->read_in_progress is -1
		 * (indicating the read from disk into the buffer is complete) when t_qread returns. the only exception
		 * is if cr->cycle is higher than the "cycle" returned by t_qread (signifying the buffer got reused for
		 * another block concurrently) in which case the cycle check in the validation logic will detect this.
		 */
		*cycle = cr->cycle;
		for (lcnt = 1;  ; lcnt++)
		{
			if (0 > cr->read_in_progress)
			{	/* it's not being read */
				if (clustered && (0 == cr->bt_index) &&
					(cr->tn < ((th_rec *)((uchar_ptr_t)csa->th_base + csa->th_base->tnque.fl))->tn))
				{	/* can't rely on the buffer */
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					break;
				}
				*cr_out = cr;
				VMS_ONLY(
					/* If we were doing the db_csh_get() above (in t_qread itself) and located the cache-record
					 * which, before coming here and taking a copy of cr->cycle a few lines above, was made an
					 * older twin by another process in bg_update (note this can happen in VMS only) which has
					 * already incremented the cycle, we will end up having a copy of the old cache-record with
					 * its incremented cycle number and hence will succeed in tp_hist validation if we return
					 * this <cr,cycle> combination although we don't want to since this "cr" is not current for
					 * the given block as of now. Note that the "indexmod" optimization in tp_tend() relies on
					 * an accurate intermediate validation by tp_hist() which in turn relies on the <cr,cycle>
					 * value returned by t_qread to be accurate for a given blk at the current point in time.
					 * We detect the older-twin case by the following check. Note that here we depend on the
					 * the fact that bg_update() sets cr->bt_index to 0 before incrementing cr->cycle.
					 * Given that order, cr->bt_index can be guaranteed to be 0 if we read the incremented cycle
					 */
					if (cr->twin && (0 == cr->bt_index))
						break;
				)
				if (cr->blk != blk)
					break;
				if (was_crit != csa->now_crit)
					rel_crit(gv_cur_region);
				assert(was_crit == csa->now_crit);
				if (reset_first_tp_srch_status)
				{	/* keep the parantheses for the if (although single line) since the following is a macro */
					RESET_FIRST_TP_SRCH_STATUS(first_tp_srch_status, cr, *cycle);
				}
				/* Note that at this point we expect t_qread to return a <cr,cycle> combination that
				 * corresponds to "blk" passed in. It is crucial to get an accurate value for both the fields
				 * since tp_hist() relies on this for its intermediate validation.
				 */
				return (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
			}
			if (blk != cr->blk)
				break;
			if (lcnt >= BUF_OWNER_STUCK && (0 == (lcnt % BUF_OWNER_STUCK)))
			{
				if (FALSE == csa->now_crit)
					grab_crit(gv_cur_region);
				if (cr->read_in_progress < -1)
				{	/* outside of design; clear to known state */
					BG_TRACE_PRO(t_qread_out_of_design);
					INTERLOCK_INIT(cr);
					assert(0 == cr->r_epid);
					cr->r_epid = 0;
				} else  if (cr->read_in_progress >= 0)
				{
					BG_TRACE_PRO(t_qread_buf_owner_stuck);
					if (0 != (blocking_pid = cr->r_epid))
					{
						if (FALSE == is_proc_alive(blocking_pid, cr->image_count))
						{	/* process gone: release that process's lock */
							assert(0 == cr->bt_index);
							if (cr->bt_index)
							{
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_bad_bt_index1);
								set_wc_blocked = TRUE;
								break;
							}
							cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
							cr->blk = CR_BLKEMPTY;
							RELEASE_BUFF_READ_LOCK(cr);
						} else
						{
							rel_crit(gv_cur_region);
							send_msg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
							send_msg(VARLSTCNT(9) ERR_BUFOWNERSTUCK, 7, process_id, blocking_pid,
								cr->blk, cr->blk, (lcnt / BUF_OWNER_STUCK),
								cr->read_in_progress, cr->rip_latch.u.parts.latch_pid);
							if ((4 * BUF_OWNER_STUCK) <= lcnt)
								GTMASSERT;
							/* Kickstart the process taking a long time in case it was suspended */
							UNIX_ONLY(continue_proc(blocking_pid));
						}
					} else
					{	/* process stopped before could set r_epid */
						assert(0 == cr->bt_index);
						if (cr->bt_index)
						{
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_bad_bt_index2);
							set_wc_blocked = TRUE;
							break;
						}
						cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
						cr->blk = CR_BLKEMPTY;
						RELEASE_BUFF_READ_LOCK(cr);
						if (cr->read_in_progress < -1)	/* race: process released since if r_epid */
							LOCK_BUFF_FOR_READ(cr, dummy);
					}
				}
				if (was_crit != csa->now_crit)
					rel_crit(gv_cur_region);
			} else
				wcs_sleep(lcnt);
		}
		if (set_wc_blocked)	/* cannot use csd->wc_blocked here as we might not necessarily have crit */
			break;
		ocnt++;
		assert((0 == was_crit) || (1 == was_crit));
		/* if we held crit while entering t_qread we might need BAD_LUCK_ABOUNDS - 1 passes.
		 * otherwise we might need BAD_LUCK_ABOUNDS passes. if we are beyond this GTMASSERT.
		 */
		if ((BAD_LUCK_ABOUNDS - was_crit) < ocnt)
		{
			assert(!csa->now_crit);
			rel_crit(gv_cur_region);
			GTMASSERT;
		}
		if (FALSE == csa->now_crit)
			grab_crit(gv_cur_region);
	} while (TRUE);
	assert(set_wc_blocked && (csd->wc_blocked || !csa->now_crit));
	rdfail_detail = cdb_sc_cacheprob;
	if (was_crit != csa->now_crit)
		rel_crit(gv_cur_region);
	assert(was_crit == csa->now_crit);
	return (sm_uc_ptr_t)NULL;
}
