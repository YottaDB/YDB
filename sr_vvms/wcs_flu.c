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
#include <psldef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbgtr.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ast.h"
#include "efn.h"
#include "jnl.h"
#include "iosp.h"
#include "sleep_cnt.h"
#include "send_msg.h"
#include "wcs_recover.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "wcs_phase2_commit_wait.h"
#include "wbox_test_init.h"
#include "memcoherency.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		process_id;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF 	jnl_gbls_t	jgbl;
GBLREF 	bool		in_backup;
#ifdef DEBUG
GBLREF	boolean_t	in_mu_rndwn_file;
#endif

error_def(ERR_DBFILERR);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLFLUSH);
error_def(ERR_TEXT);
error_def(ERR_WCBLOCKED);

static	const	unsigned short	zero_fid[3];

boolean_t wcs_flu(uint4 options)
{
	bool			broken, ret, was_crit;
	boolean_t		jnl_enabled, flush_hdr, write_epoch, sync_epoch, in_commit;
	cache_que_head		*crq, *crqwip;
	cache_rec		*cr, *crtop;
	file_control		*fc;
	sgmnt_addrs		*csa;
	sgmnt_data		*csd;
	short			iosb[4];
	uint4			jnl_status;
	unsigned int		lcnt1, lcnt2, lcnt3, pass, status;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	node_local_ptr_t        cnl;
#	ifdef DEBUG
	cache_que_head		lclwip, lclact;
	cache_rec		lclcr;
#	endif

	flush_hdr = options & WCSFLU_FLUSH_HDR;
	write_epoch = options & WCSFLU_WRITE_EPOCH;
	sync_epoch = options & WCSFLU_SYNC_EPOCH;
	/* WCSFLU_IN_COMMIT bit is set if caller is t_end or tp_tend. In that case, we should NOT invoke wcs_recover if we
	 * encounter an error. Instead we should return the error as such so they can trigger appropriate error handling.
	 * This is necessary because t_end and tp_tend could have pinned one or more cache-records (cr->in_cw_set non-zero)
	 * BEFORE invoking wcs_flu. And code AFTER the wcs_flu in them relies on the fact that those cache records stay
	 * pinned. If wcs_flu invokes wcs_recover, it will reset cr->in_cw_set to 0 for ALL cache-records so code AFTER
	 * the wcs_flu in the caller will fail because no buffer is pinned at that point.
	 */
	in_commit = options & WCSFLU_IN_COMMIT;
	csa = &(FILE_INFO(gv_cur_region)->s_addrs);
	csd = csa->hdr;
	cnl = csa->nl;
	assert(cnl->glob_sec_init);
	INCR_GVSTATS_COUNTER(csa, cnl, n_db_flush, 1);
	if (!(was_crit = csa->now_crit))	/* Caution: assignment */
		grab_crit(gv_cur_region);
	if (dba_mm == csa->hdr->acc_meth)
	{
		if (SS$_NORMAL == (ret = sys$updsec(csa->db_addrs, NULL, PSL$C_USER, 0, efn_immed_wait, iosb, NULL, 0)))
		{
			sys$synch(efn_immed_wait, iosb);
			ret = iosb[0];
		} else  if (SS$_NOTMODIFIED == ret)
			ret = SS$_NORMAL;
		if (!was_crit)
			rel_crit(gv_cur_region);
		return (SS$_NORMAL == ret);
	}
	cnl->wcsflu_pid = process_id;
	assert(dba_bg == csa->hdr->acc_meth);
	/* Worry about journaling only if JNL_ENABLED and if journal has been opened in shared memory */
	jnl_enabled = (JNL_ENABLED(csa->hdr) && (0 != memcmp(cnl->jnl_file.jnl_file_id.fid, zero_fid, SIZEOF(zero_fid))));
	if (jnl_enabled)
	{
		jpc = csa->jnl;
		jbp = jpc->jnl_buff;
		/* Assert that we never flush the cache in the midst of a database commit. The only exception is MUPIP RUNDOWN */
		assert((csa->ti->curr_tn == csa->ti->early_tn) || in_mu_rndwn_file);
		if (!jgbl.dont_reset_gbl_jrec_time)
			SET_GBL_JREC_TIME;	/* needed before jnl_ensure_open */
		/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time (if needed) to maintain time order of jnl
		 * records. This needs to be done BEFORE the jnl_ensure_open as that could write journal records
		 * (if it decides to switch to a new journal file)
		 */
		ADJUST_GBL_JREC_TIME(jgbl, jbp);
		assert(csa == cs_addrs);	/* for jnl_ensure_open */
		jnl_status = jnl_ensure_open();
		if (0 != jnl_status)
		{
			assert(ERR_JNLFILOPN == jnl_status);
			cnl->wcsflu_pid = 0;
			if (!was_crit)
				rel_crit(gv_cur_region);
			send_msg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
			return FALSE;
		}
		assert(NOJNL != jpc->channel);
		if (SS_NORMAL != (jnl_status = jnl_flush(gv_cur_region)))
		{
			assert(NOJNL == jpc->channel); /* jnl file lost */
			if (!was_crit)
				rel_crit(gv_cur_region);
			send_msg(VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
				ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush during wcs_flu"),
				jnl_status);
			return FALSE;
		}
	}
	/* If not mupip rundown, wait for ALL active phase2 commits to complete first.
	 * In case of mupip rundown, we know no one else is accessing shared memory so no point waiting.
	 */
	assert(!in_mu_rndwn_file || (0 == cnl->wcs_phase2_commit_pidcnt));
	if (cnl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(csa, NULL))
	{
		if (!was_crit)
			rel_crit(gv_cur_region);
		return FALSE;	/* we expect the caller to trigger cache-recovery which will fix this counter */
	}
	/* Now that all concurrent commits are complete, wait for these dirty buffers to be flushed to disk. */
	crq = &csa->acc_meth.bg.cache_state->cacheq_active;
	crqwip = &csa->acc_meth.bg.cache_state->cacheq_wip;
	for (pass = 1, ret = FALSE; FALSE == ret; pass++)
	{
		for (lcnt1 = DIVIDE_ROUND_UP(csd->n_bts, csd->n_wrt_per_flu);  (0 != crq->fl);  lcnt1--)
		{	/* attempt to clear the active queue */
			if (SS$_NORMAL != (status = sys$dclast(wcs_wtstart, gv_cur_region, 0)))
			{
				send_msg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), status);
				assert(FALSE);
				status = sys$setast(DISABLE);
				wcs_wtstart(gv_cur_region);
				if (SS$_WASSET == status)
					ENABLE_AST;
			}
			if (!wcs_wtfini(gv_cur_region) || (0 == lcnt1))
				break;
		}
		assert((1 == pass) || (0 == cnl->in_wtstart));	/* in second pass there should be no active writers */
		/* Wait for all active writers to finish. We wait for 1 minute (similar to code in wcs_recover) */
		SIGNAL_WRITERS_TO_STOP(cnl);	/* to stop all active writers */
		WAIT_FOR_WRITERS_TO_STOP(cnl, lcnt2, MAXWTSTARTWAIT);
		SIGNAL_WRITERS_TO_RESUME(cnl);
		/* Attempt to clear the wip queue and double check that all is well */
		cr = &csa->acc_meth.bg.cache_state->cache_array;
		cr += csd->bt_buckets;
		crtop = cr + csd->n_bts;
		for (lcnt3 = 0, broken = FALSE;  FALSE == ret; )
		{
			for ( ; cr < crtop; cr++)
			{	/* check that nothing is dirty */
				if (cr->dirty)
				{
					broken = TRUE;
					if (0 != crqwip->fl)
					{
						broken = ((!wcs_wtfini(gv_cur_region)) ? TRUE : FALSE);
						assert(FALSE == broken);
						if ((FALSE == broken) && !cr->dirty)
							continue;
					}
					if (0 != crq->fl)
					{
						broken = FALSE;
						if (SS$_NORMAL != (status = sys$dclast(wcs_wtstart, gv_cur_region, 0)))
						{
							send_msg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), status);
							assert(FALSE);
							status = sys$setast(DISABLE);
							wcs_wtstart(gv_cur_region);
							if (SS$_WASSET == status)
								ENABLE_AST;
						}
					}
					/* This means we found a dirty cache-record that is neither in the active or wip queue.
					 * This is possible in the following situations.
					 * 	a) If crash shutdown and mupip rundown is invoked.
					 * 	b) If a process encountered an error in the midst of committing in phase2
					 * 	   and secshr_db_clnup completed the commit for it. That would not have
					 * 	   inserted the cr into the queues (see comment there as to why). But in
					 * 	   that case, it would have set cnl->wc_blocked to TRUE. Unfortunately, we
					 * 	   reset c>nl->wc_blocked to FALSE as part of the SIGNAL_WRITERS_TO_RESUME
					 * 	   macro call (a few line above). So the only test that we can do is that
					 * 	   a phase2 commit error occurred. This is tested by checking that the
					 * 	   variable gtm_white_box_test_case_enabled is non-zero.
					 */
					assert((FALSE == broken) || in_mu_rndwn_file || gtm_white_box_test_case_enabled);
					break;
				}
			}
			if (FALSE == (ret = !broken))
				break;
			if (FALSE == (ret = (cr == crtop)))
			{	/* didn't make it to the top without a dirty */
				if (++lcnt3 > BUF_OWNER_STUCK)
				{
					DEBUG_ONLY(
						lclcr = *cr;
						lclwip = *crqwip;
						lclact = *crq;
					)
					break;
				}
				else if (0 < lcnt3)
					wcs_sleep(lcnt3);
			}
		}
		if (FALSE == ret)
		{	/* something wrong */
			/* The only case we know of currently when this is possible is if a process encountered an error
			 * in the midst of committing in phase2 and secshr_db_clnup completed the commit for it and set
			 * wc_blocked to TRUE (even though it was out of crit) causing the wcs_wtstart calls done above
			 * to do nothing. But phase2 commit errors are currently enabled only through white-box testing.
			 * The only exception to this is if this is a crash shutdown and later mupip rundown is being
			 * invoked on this shared memory. Assert accordingly.
			 */
			assert(gtm_white_box_test_case_enabled || in_mu_rndwn_file);
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wcb_wcs_flu1);
                	send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_wcs_flu1"),
                        	process_id, &csa->ti->curr_tn, DB_LEN_STR(gv_cur_region));
			if (in_commit)
			{	/* We should NOT be invoking wcs_recover as otherwise the callers (t_end or tp_tend)
				 * will get confused (see explanation above where variable "in_commit" gets set).
				 */
				assert(was_crit);	/* so dont need to rel_crit */
				return FALSE;
			}
			if (pass > 1)
				GTMASSERT;
			wcs_recover(gv_cur_region);
		}
	}
	if (flush_hdr)
		fileheader_sync(gv_cur_region);
	if (jnl_enabled && write_epoch && jbp->before_images)
	{	/* Parallel code in Unix does an fsync. Not needed here since VMS writes are hard writes */
		assert(jgbl.gbl_jrec_time);
		if (!jgbl.mur_extract)
		{
			if (0 == jpc->pini_addr)
				jnl_put_jrt_pini(csa);
			jnl_write_epoch_rec(csa);
			INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_epoch_regular, 1);
		}
	}
	cnl->last_wcsflu_tn = csa->ti->curr_tn;	/* record when last successful wcs_flu occurred */
	cnl->wcsflu_pid = 0;
	if (!was_crit)
		rel_crit(gv_cur_region);
	/* sync the epoch record in the journal if needed. */
	if (jnl_enabled && jbp->before_images &&
			write_epoch && sync_epoch && (csa->ti->curr_tn == csa->ti->early_tn))
	{	/* Note that if we are in the midst of committing and came here through a bizarre
		 * stack trace (like wcs_get_space etc.) we want to defer syncing to when we go out of crit.
		 * Note that we are guaranteed to come back to wcs_wtstart since we are currently in commit-phase
		 * and will dirty atleast one block for a timer to be triggered.
		 */
		jnl_wait(gv_cur_region);
	}
	return ret;
}
