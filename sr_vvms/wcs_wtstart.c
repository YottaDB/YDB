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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ast.h"
#include "interlock.h"
#include "jnl.h"
#include "relqueopi.h"
#include "send_msg.h"
#include "iosp.h"
#include "gdsbgtr.h"		/* for the BG_TRACE_ANY macros */
#include "memcoherency.h"

GBLREF	short	astq_dyn_avail;
GBLREF	uint4	process_id;
GBLREF	uint4	image_count;

error_def(ERR_BLKWRITERR);
error_def(ERR_DBCCERR);
error_def(ERR_DBFILERR);

/* currently returns 0 always */
int4	wcs_wtstart(gd_region *reg)
{
	boolean_t		bmp_status;
	blk_hdr_ptr_t		bp;
	cache_que_head_ptr_t	ahead, whead;
	cache_state_rec_ptr_t	csr, start_csr;
	jnl_private_control	*jpc;
	node_local_ptr_t	cnl;
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	unsigned int		max_writes, wcnt;
	int			status, dummy, lcnt, n;
	cache_rec_ptr_t		cr, cr_lo, cr_hi;
	uint4			index;

        assert(lib$ast_in_prog());	/* if a dclast fails and the setast is used, this assert fails - put can't happen in pro */
	assert(0 != WRT_STRT_PNDNG);
	assert(0 == (1 & WRT_STRT_PNDNG));
	assert(FALSE == reg->read_only);
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	if (cnl->wc_blocked)
		return 0;
	INCR_INTENT_WTSTART(cnl);	/* signal intent to enter wcs_wtstart */
	/* the above interlocked instruction does the appropriate write memory barrier to publish this change to the world */
	SHM_READ_MEMORY_BARRIER;	/* need to do this to ensure uptodate value of csa->nl->wc_blocked is read */
	if (cnl->wc_blocked)
	{
		DECR_INTENT_WTSTART(cnl);
		return 0;
	}
	csa->in_wtstart = TRUE;			/* secshr_db_clnup depends on the order of this and the INCR_CNT done below */
	INCR_CNT(&cnl->in_wtstart, &dummy);	/* if a wc_blocked sneeks in the loop below will prevent queue operations */
	SAVE_WTSTART_PID(cnl, process_id, index);
	assert(cnl->in_wtstart > 0 && csa->in_wtstart);

	jpc = csa->jnl;
	assert(!JNL_ALLOWED(csd) || NULL != jpc);
	if (JNL_ENABLED(csd) && (NULL != jpc) && (NOJNL != jpc->channel)) /* not jnl_write, which is believed to be ok */
		jnl_start_ast(jpc);
	cnl->wcs_staleness = -1;
	lcnt = csd->n_bts;
	max_writes = csd->n_wrt_per_flu;
	whead = &csa->acc_meth.bg.cache_state->cacheq_wip;
	assert(0 == ((int4)whead & 7));
	ahead = &csa->acc_meth.bg.cache_state->cacheq_active;
	assert(0 == ((int4)ahead & 7));
	cr_lo = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	cr_hi = cr_lo + csd->n_bts;
	csa->wbuf_dqd++;	/* increase the counter. In case ACCVIO or something bad happens
				 * secshr_db_cleanup will check the field and handle appropriarely */
	for (wcnt = 0, start_csr = NULL;  (0 < lcnt) && (wcnt < max_writes) &&  (FALSE == cnl->wc_blocked);  --lcnt)
	{
		csr = (cache_state_rec_ptr_t)REMQHI((que_head_ptr_t)ahead);
		if (INTERLOCK_FAIL == (int4)csr)
		{
			assert(FALSE);
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wcb_wtstart_lckfail1);
			break;
		}
		if (NULL == csr)
			break;		/* the queue is empty */
		/* wcs_get_space relies on the fact that a cache-record that is out of either active or wip queue has its
		 * fl and bl fields set to 0. Initialize those fields now that this cache-record is out of the active queue.
		 */
		csr->state_que.fl = csr->state_que.bl = 0;
		if (csr == start_csr)
		{	/* completed a tour of the queue */
			status = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)ahead);
			if (INTERLOCK_FAIL == status)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtstart_lckfail2);
			}
			break;
		}
		cr = (cache_rec_ptr_t)((sm_uc_ptr_t)csr - SIZEOF(cr->blkque));
		assert(!CR_NOT_ALIGNED(cr, cr_lo) && !CR_NOT_IN_RANGE(cr, cr_lo, cr_hi));
		if (CR_BLKEMPTY == csr->blk)
		{	/* must be left by t_commit_cleanup - remove it from the queue and complete the cleanup */
			assert(0 == csr->twin);
			assert(FALSE == csr->data_invalid);
			assert(csr->dirty);
			assert(0 == csr->iosb.cond);
			csr->dirty = 0;
			INCR_CNT(&cnl->wc_in_free, &dummy);
			if (!SUB_ENT_FROM_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock)
							&& !cnl->wcsflu_pid && FALSE == csa->dbsync_timer)
			{
				assert(0 < astq_dyn_avail);
				if (0 < astq_dyn_avail)
				{
					csa->dbsync_timer = TRUE;
					astq_dyn_avail--;
					/* Since we are already in an ast, we can invoke wcs_clean_dbsync_timer_ast directly. */
					wcs_clean_dbsync_timer_ast(csa);
				}
			}
			continue;
		}
		assert(0 != csr->dirty);
		assert(0 == csr->iosb.cond);
		assert(0 == csr->epid);
		assert(0 == csr->r_epid);
		if (((0 != csr->twin) && (1 != ((cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, csr->twin))->iosb.cond))
			|| ((0 != csr->jnl_addr) && JNL_ENABLED(csd) && (csr->jnl_addr > jpc->jnl_buff->dskaddr)))
		{	/* twin still in write OR would write ahead of journal */
			status = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)ahead);
			if ((unsigned int)INTERLOCK_FAIL == status)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtstart_lckfail3);
				break;
			}
			if (NULL == start_csr)
				start_csr = csr;
			continue;
		}
		csr->image_count = image_count;
		csr->epid = process_id;
		csr->iosb.cond = WRT_STRT_PNDNG;		/* Illegal value, shows that write has not been issued */
		LOCK_BUFF_FOR_WRITE(csr, n);
		assert(WRITE_LATCH_VAL(csr) >= LATCH_CLEAR);
		assert(WRITE_LATCH_VAL(csr) <= LATCH_CONFLICT);
		if (n < 1)
		{	/* sole owner; if not, leave it off the queue for t_end_sysops to replace */
			assert(WRITE_LATCH_VAL(csr) > LATCH_CLEAR);
			assert(FALSE == csr->data_invalid);	/* check that buffer has valid data */
			assert(0 == n);
			assert(csr->epid);
			status = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)whead);
			if (INTERLOCK_FAIL == status)
			{
				assert(FALSE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wtstart_lckfail4);
				break;
			}
			INCR_GVSTATS_COUNTER(csa, cnl, n_dsk_write, 1);
			CR_BUFFER_CHECK1(reg, csa, csd, cr, cr_lo, cr_hi);
			bp = (blk_hdr_ptr_t)(GDS_ANY_REL2ABS(csa, csr->buffaddr));
			VALIDATE_BM_BLK(csr->blk, bp, csa, reg, bmp_status);	/* bmp_status reflects bitmap buffer's validity */
			if (SUB_ENT_FROM_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock) || cnl->wcsflu_pid)
				status = dsk_write(reg, csr->blk, cr, NULL, 0, &csr->iosb);
			else
			{
				if ((0 < astq_dyn_avail) && (FALSE == csa->dbsync_timer))
				{
					astq_dyn_avail--;
					csa->dbsync_timer = TRUE;
					status = dsk_write(reg, csr->blk, cr, wcs_clean_dbsync_timer_ast, csa, &csr->iosb);
				} else
				{
					assert(csa->dbsync_timer);	/* in PRO, we skip writing an epoch record. */
					status = dsk_write(reg, csr->blk, cr, NULL, 0, &csr->iosb);
				}
			}
			if (0 == (status & 1))
			{	/* if it fails, leave it and hope that another time, another process will work (infinite retry) */
				send_msg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
				send_msg(VARLSTCNT(3) ERR_BLKWRITERR, 1, csr->blk);
				send_msg(VARLSTCNT(1) status);
				/* since the state of iosb.cond is indeterminate (but believed to never have severity of SUCCESS)
				 * when the qio fails, the following [slightly sleazy] assignment forces a retry */
				csr->iosb.cond = WRT_STRT_PNDNG;/* this and the next line assume that iosb and epid are aligned */
				csr->epid = 0;			/* so that memory coherency is not disrupted by concurrent access */
				assert(FALSE);
			}
			++wcnt;
		}
	}
	csa->wbuf_dqd--;	/* Everything completed successfully, so clear the field now */
	assert(cnl->in_wtstart > 0 && csa->in_wtstart);
	DECR_CNT(&cnl->in_wtstart, &dummy);		/* secshr_db_clnup depends on the order of this and the next line */
	CLEAR_WTSTART_PID(cnl, index);
	csa->in_wtstart = FALSE;
	DECR_INTENT_WTSTART(cnl);
	/* Ideally we should be having an invocation of the DEFERRED_EXIT_HANDLING_CHECK macro here (see unix wcs_wtstart.c).
	 * But since we are in an AST at this point, invoking the exit handler is going to defer it once again (there is
	 * code in generic_exit_handler.c which checks for ast_in_prog) so no point invoking it here. This means if
	 * a MUPIP STOP gets delivered while we are in wcs_wtstart and therefore exit-handling gets deferred, it will not
	 * be triggered at the end of wcs_wtstart but has to wait until the next t_end/tp_tend/rel_crit/rel_lock occurs
	 * (those places have the DEFERRED_EXIT_HANDLING_CHECK checks).
	 */
	return 0;
}
