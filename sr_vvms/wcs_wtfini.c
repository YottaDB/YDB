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

#include <ssdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"		/* for the BG_TRACE_ANY macros */
#include "filestruct.h"		/* for WRT_STRT_PNDNG and FILE_INFO macros */
#include "ast.h"		/* for ENABLE and DISABLE macros to pass to sys$setast() */
#include "interlock.h"		/* atleast for *_LATCH_* macros */
#include "relqueopi.h"		/* for REMQHI and INSQHI macros */
#include "send_msg.h"
#include "is_proc_alive.h"
#include "shmpool.h"

GBLREF	int4		wtfini_in_prog;
GBLREF	uint4		image_count;
GBLREF	uint4		process_id;

error_def(ERR_BLKWRITERR);
error_def(ERR_DBCCERR);
error_def(ERR_DBFILERR);
error_def(ERR_IOWRITERR);

bool	wcs_wtfini(gd_region *reg)
{
	cache_state_rec_ptr_t	csr, start_csr;
	cache_rec_ptr_t		cr_alt;
	cache_que_head_ptr_t	whead;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	sm_off_t		sblkh_off;
	unsigned int		ast_status, dummy, lcnt;
	uint4			wrtfail_epid;
	int			status;
	cache_rec_ptr_t		cr, cr_lo, cr_hi;
	boolean_t		ret_value;
	unsigned int		iosb_cond;
	node_local_ptr_t	cnl;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	BG_TRACE_ANY(csa, wcs_wtfini_invoked);
	wtfini_in_prog++;
	assert((TRUE == csa->now_crit) || (TRUE == csd->clustered));
	whead = &csa->acc_meth.bg.cache_state->cacheq_wip;
	assert(0 == (((int4)whead) & 7));
	cr_lo = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	cr_hi = cr_lo + csd->n_bts;
	ret_value = TRUE;
	for (lcnt = 0, start_csr = NULL;  lcnt <= csd->n_bts;  lcnt++)
	{
		csr = (cache_state_rec_ptr_t)REMQHI((que_head_ptr_t)whead);
		if (INTERLOCK_FAIL == (int4)csr)
		{
			assert(FALSE);
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wcb_wtfini_lckfail1);
			ret_value = FALSE;
			break;
		}
		if (NULL == csr)
			break;		/* empty queue */
		/* wcs_get_space relies on the fact that a cache-record that is out of either active or wip queue has its
		 * fl and bl fields set to 0. Initialize those fields now that this cache-record is out of the active queue.
		 */
		csr->state_que.fl = csr->state_que.bl = 0;
		if (csr == start_csr)
		{
			status = INSQHI((que_ent_ptr_t)csr, (que_head_ptr_t)whead);
			if (INTERLOCK_FAIL == status)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtfini_lckfail2);
				ret_value = FALSE;
			}
			break;		/* looped the queue */
		}
		cr = (cache_rec_ptr_t)((sm_uc_ptr_t)csr - SIZEOF(cr->blkque));
		if (CR_NOT_ALIGNED(cr, cr_lo) || CR_NOT_IN_RANGE(cr, cr_lo, cr_hi))
		{
			assert(FALSE);
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wc_blocked_wcs_wtfini_bad_cr);
			ret_value = FALSE;
			break;
		}
		assert(0 == csr->r_epid);
		assert(LATCH_CLEAR < WRITE_LATCH_VAL(csr));
		iosb_cond = csr->iosb.cond;
		/* Since cr->iosb.cond can change concurrently (wcs_wtstart.c can issue a dsk_write for the same cache-record
		 * concurrently which can change the iosb) and since it needs to be used in multiple places in the if statement
		 * below, we note down cr->iosb.cond in a local variable and use that instead. Using the shared memory value
		 * might cause us to go into the if block for a block whose IO is not complete and end up reissuing the IO for
		 * the same block causing TWO CONCURRENTLY PENDING IOs for the same block.
		 */
		if ((((0 != iosb_cond)
				&& ((WRT_STRT_PNDNG != iosb_cond) || (FALSE == is_proc_alive(csr->epid, csr->image_count))))
				|| ((TRUE == csr->wip_stopped) && (FALSE == is_proc_alive(csr->epid, csr->image_count)))))
		{	/* if 0 == csr->epid, is_proc_alive returns FALSE */
			/* As long as the cache-record is PINNED (in_cw_set is TRUE), wcs_wtfini should NOT remove an older
			 * twin even if it is an older twin whose write is complete. This is because the contents of that
			 * buffer could be relied upon by secshr_db_clnup/wcs_recover to complete the flush of the before-image
			 * to the backup file (in case of an error in the midst of commit) so we should NOT touch csr->blk.
			 */
			if (1 == (iosb_cond & 1) || (0 == csr->dirty) || (CR_BLKEMPTY == csr->blk)
				|| ((0 == csr->bt_index) && !csr->in_cw_set))
			{	/* it's done properly, or it doesn't matter */
				if (0 != csr->twin)
				{
					cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, csr->twin);
					assert(&((cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->twin))->state_que == csr);
					cr_alt->twin = csr->twin = 0;
					if (0 == csr->bt_index)
					{
						assert(CR_BLKEMPTY != cr_alt->blk);
						assert(LATCH_CONFLICT == WRITE_LATCH_VAL(csr));
						csr->cycle++;	/* increment cycle whenever blk number changes (tp_hist needs it) */
						csr->blk = CR_BLKEMPTY;
					} else
					{
						assert(CR_BLKEMPTY != csr->blk);
						cr_alt->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
						cr_alt->blk = CR_BLKEMPTY;
					}
				}
				assert(FALSE == csr->data_invalid);
				BG_TRACE_ANY(csa, qio_to_clean);
				csr->flushed_dirty_tn = csr->dirty;
				csr->dirty = 0;
				csr->epid = 0;
				csr->iosb.cond = 0;
				csr->wip_stopped = FALSE;
				INCR_CNT(&cnl->wc_in_free, &dummy);
				WRITE_LATCH_VAL(csr) = LATCH_CLEAR; /* off the queues and now_crit */
				SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
			} else
			{	/* block is still valid, current, dirty and the write was not successful OR in_cw_set is TRUE */
				status = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)whead);
				if (INTERLOCK_FAIL == status)
				{
					assert(FALSE);
					SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wcb_wtfini_lckfail3);
					ret_value = FALSE;
					break;
				}
				if ((FALSE == reg->read_only) && !lib$ast_in_prog())
				{	/* Don't want to do setast(DISABLE)s when we are within an AST.
					 * Anyway a future non-AST driven wcs_wtfini will take care of this.
					 */
					wrtfail_epid = csr->epid;
					csr->image_count = image_count;
					csr->epid = process_id;
					csr->iosb.cond = WRT_STRT_PNDNG;
					csr->wip_stopped = FALSE;
					csr->shmpool_blk_off = 0;	/* dsk_write() may (if dwngrd) want to redo this anyway */
					ast_status = sys$setast(DISABLE);
					/* Notify of IO error. Notify of status if not special retry and the original job died. */
					send_msg(VARLSTCNT(7) ERR_IOWRITERR, 5, wrtfail_epid, csr->blk, DB_LEN_STR(reg), csr->epid);
					if (WRT_STRT_PNDNG != iosb_cond)
						send_msg(VARLSTCNT(1) iosb_cond);
					CR_BUFFER_CHECK1(reg, csa, csd, cr, cr_lo, cr_hi);
					status = dsk_write(reg, csr->blk, cr, NULL, 0, &csr->iosb);
					if (SS$_WASSET == ast_status)
						sys$setast(ENABLE);
					if (0 == (status & 1))
					{	/* if it fails, leave it and hope that another process will work (infinite retry) */
						send_msg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
						send_msg(VARLSTCNT(3) ERR_BLKWRITERR, 1, csr->blk);
						send_msg(VARLSTCNT(1) status);
						/* since the state of iosb.cond is indeterminate (but believed to never have
						 * severity of SUCCESS) the following [slightly sleazy] assignment forces a retry */
						csr->iosb.cond = WRT_STRT_PNDNG;
						csr->epid = 0;
						csr->shmpool_blk_off = 0; /* Allow reuse of our reformat buffer (if any) */
						assert(FALSE);
					}
				}
				if (NULL == start_csr)
					start_csr = csr;
			}
		} else
		{
			assert(csr->epid);
			status = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)whead);
			if (INTERLOCK_FAIL == status)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtfini_lckfail4);
				ret_value = FALSE;
				break;
			}
			if (NULL == start_csr)
				start_csr = csr;
		}
	}
	wtfini_in_prog--;
	assert(0 <= wtfini_in_prog);
	if (0 > wtfini_in_prog)
		wtfini_in_prog = 0;
	if (!ret_value || ((NULL != csr) && (csr != start_csr)))
	{
		assert(FALSE);
		return FALSE;
	}
	return TRUE;
}
