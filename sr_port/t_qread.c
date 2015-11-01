/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "gdsbgtr.h"
#include "sleep_cnt.h"
#include "send_msg.h"
#include "t_qread.h"
#include "gvcst_blk_build.h"
#include "mm_read.h"
#include "is_proc_alive.h"
#include "cache.h"
#include "cws_insert.h"
#include "wcs_sleep.h"
#include "add_inter.h"

GBLDEF srch_blk_status	*first_tp_srch_status;	/* the first srch_blk_status for this block in this transaction */
GBLDEF unsigned char	rdfail_detail;	/* t_qread uses a 0 return to indicate a failure (no buffer filled) and the real
					status of the read is returned using a global reference, as the status detail
					should typically not be needed and optimizing the call is important */

GBLREF bool             certify_all_blocks;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF short		crash_count;
GBLREF short		dollar_tlevel;
GBLREF unsigned int	t_tries;
GBLREF uint4		process_id;

DEBUG_ONLY(GBLREF gv_namehead	*gv_target;)

#define BAD_LUCK_ABOUNDS 1
#define	RESET_FIRST_TP_SRCH_STATUS(first_tp_srch_status, newcr, newcycle)				\
	assert((first_tp_srch_status)->cr != (newcr) || (first_tp_srch_status)->cycle != (newcycle));	\
	(first_tp_srch_status)->cr = (newcr);								\
	(first_tp_srch_status)->cycle = (newcycle);							\
	(first_tp_srch_status)->buffaddr = (sm_uc_ptr_t)GDS_REL2ABS((newcr)->buffaddr);			\


sm_uc_ptr_t t_qread(block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr_out)
	/* cycle is used in t_end to detect if the buffer has been refreshed since the t_qread */
{
	uint4			status, duint4;
	cache_rec_ptr_t		cr;
	bt_rec_ptr_t		bt;
	bool			clustered, was_crit;
	int			dummy, lcnt, ocnt;
	cw_set_element		*cse;
	off_chain		chain1;
	register sgmnt_addrs	*csa;
	register sgmnt_data_ptr_t	csd;
	int4			dummy_errno;
	boolean_t		already_built, is_mm, reset_first_tp_srch_status;

	error_def(ERR_DBFILERR);
	error_def(ERR_BUFOWNERSTUCK);

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
				first_tp_srch_status = (srch_blk_status *)lookup_hashtab_ent(sgm_info_ptr->blks_in_use,
												(void *)blk, &duint4);
				ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(first_tp_srch_status, sgm_info_ptr);
				cse = first_tp_srch_status ? first_tp_srch_status->ptr : NULL;
			}
			assert(!cse || !cse->high_tlevel);
			if (cse)
			{	/* transaction has modified the sought after block  */
				assert(gds_t_writemap != cse->mode);
				if (FALSE == cse->done)
				{	/* out of date, so make it current */
					already_built = (NULL != cse->new_buff);
					gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, 0);
					assert(cse->blk_target);
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
						if ((!is_mm) && (first_tp_srch_status->cycle != first_tp_srch_status->cr->cycle
							|| first_tp_srch_status->blk_num != first_tp_srch_status->cr->blk))
						{
							assert(CDB_STAGNATE > t_tries);
							rdfail_detail = cdb_sc_lostcr;	/* should this be something else */
							return (sm_uc_ptr_t)NULL;
						}
        					if (certify_all_blocks &&
								FALSE == cert_blk(blk, (blk_hdr_ptr_t)cse->new_buff,
									cse->blk_target->root))
							GTMASSERT;
					}
					cse->done = TRUE;
				}
				*cycle = CYCLE_PVT_COPY;
				*cr_out = 0;
				return (sm_uc_ptr_t)cse->new_buff;
			}
			assert(!chain1.flag);
		} else
			first_tp_srch_status =
					(srch_blk_status *)lookup_hashtab_ent(sgm_info_ptr->blks_in_use, (void *)blk, &duint4);
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
					cws_insert(blk);
				return (sm_uc_ptr_t)first_tp_srch_status->buffaddr;
			} else
			{	/* Block was already part of the read-set of this transaction, but got recycled. Allow for
				 * recycling. But update the first_tp_srch_status (for this blk) in the si->first_tp_hist
				 * array to reflect the new buffer, cycle and cache-record. Since we know those only at the end of
				 * t_qread, set a variable here that will enable the updation before returning from t_qread().
				 */
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
					break;
				assert(0 <= cr->read_in_progress);
				*cycle = cr->cycle;
				cr->tn = csa->ti->curr_tn;
				if (FALSE == was_crit)
					rel_crit(gv_cur_region);
				/* read outside of crit may be of a stale block but should be detected by t_end or tp_tend */
				assert(0 == cr->dirty);
				assert(cr->read_in_progress >= 0);
				INCR_DB_CSH_COUNTER(csa, n_dsk_reads, 1);
				if (SS_NORMAL != (status = dsk_read(blk, GDS_REL2ABS(cr->buffaddr))))
				{
					RELEASE_BUFF_READ_LOCK(cr);
					assert(was_crit == csa->now_crit);
					if (FUTURE_READ == status)
					{	/* in cluster, block can be in the "future" with respect to the local history */
						assert(TRUE == clustered);
						assert(FALSE == csa->now_crit);
						rdfail_detail = cdb_sc_future_read;	/* t_retry forces the history up to date */
						return (sm_uc_ptr_t)NULL;
					}
					rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), status);
				}
				assert(0 <= cr->read_in_progress);
				assert(0 == cr->dirty);
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
			break;
		for (lcnt = 1;  ; lcnt++)
		{
			if (0 > cr->read_in_progress)
			{	/* it's not being read */
				if (clustered && (0 == cr->bt_index) &&
					(cr->tn < ((th_rec *)((unsigned char *)csa->th_base + csa->th_base->tnque.fl))->tn))
				{	/* can't rely on the buffer */
					cr->blk = CR_BLKEMPTY;
					break;
				}
				*cycle = cr->cycle;
				*cr_out = cr;
				if (cr->blk != blk)
					break;
				if (was_crit != csa->now_crit)
					rel_crit(gv_cur_region);
				assert(was_crit == csa->now_crit);
				if (reset_first_tp_srch_status)
				{	/* keep the parantheses for the if (although single line) since the following is a macro */
					RESET_FIRST_TP_SRCH_STATUS(first_tp_srch_status, cr, *cycle);
				}
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
					if (0 != cr->r_epid)
					{
						if (FALSE == is_proc_alive(cr->r_epid, cr->image_count))
						{	/* process gone: release that process's lock */
							cr->blk = CR_BLKEMPTY;
							RELEASE_BUFF_READ_LOCK(cr);
						} else
						{
							rel_crit(gv_cur_region);
							send_msg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
							send_msg(VARLSTCNT(9) ERR_BUFOWNERSTUCK, 7, process_id, cr->r_epid,
								cr->blk, cr->blk, (lcnt / BUF_OWNER_STUCK),
								cr->read_in_progress, cr->rip_latch.latch_pid);
							if ((4 * BUF_OWNER_STUCK) <= lcnt)
								GTMASSERT;
						}
					} else
					{	/* process stopped before could set r_epid */
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
		ocnt++;
		if (BAD_LUCK_ABOUNDS <= ocnt)
		{
			if (BAD_LUCK_ABOUNDS < ocnt || csa->now_crit)
			{
				rel_crit(gv_cur_region);
				GTMASSERT;
			}
			if (FALSE == csa->now_crit)
				grab_crit(gv_cur_region);
		}
	} while (TRUE);
	BG_TRACE_PRO_ANY(csa, wc_blocked_t_qread_db_csh_get_invalid_blk);
	SET_TRACEABLE_VAR(cs_data->wc_blocked, TRUE);
	rdfail_detail = cdb_sc_cacheprob;
	if (was_crit != csa->now_crit)
		rel_crit(gv_cur_region);
	assert(was_crit == csa->now_crit);
	return (sm_uc_ptr_t)NULL;
}
