/****************************************************************
 *								*
 *	Copyright 2007, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_facility.h"
#include "gdsroot.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "interlock.h"
#include "jnl.h"
#include "sleep_cnt.h"
#include "gdsbgtr.h"

#include "wcs_get_space.h"
#include "wbox_test_init.h"
#include "memcoherency.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_region		*gv_cur_region;	/* needed for the JNL_ENSURE_OPEN_WCS_WTSTART macro */

/* go after a specific number of buffers or a particular buffer */
bool	wcs_get_space(gd_region *reg, int needed, cache_rec *cr)
{
	unsigned int		lcnt, ocnt, status;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t        cnl;
	que_ent_ptr_t		base, q0;
	int4			dummy_errno;
	boolean_t		is_mm;

	assert((0 != needed) || (NULL != cr));
	csa = &(FILE_INFO(reg)->s_addrs);
	assert(csa == cs_addrs);
	csd = csa->hdr;
	is_mm = (dba_mm == csd->acc_meth);
	assert(is_mm || (dba_bg == csd->acc_meth));
	cnl = csa->nl;
	if (FALSE == csa->now_crit)
	{
		assert(0 != needed);	/* if needed == 0, then we should be in crit */
		for (lcnt = DIVIDE_ROUND_UP(needed, csd->n_wrt_per_flu);  0 < lcnt;  lcnt--)
			JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, dummy_errno);
					/* a macro that ensure jnl is open, dclast's wcs_wtstart and checks for errors etc. */
		return TRUE;
	}
	if (FALSE == wcs_wtfini(reg))
		return FALSE;
	/* while calculating flush_trigger, the decrement should be atleast 1 if still not reached the minimum allowed */
	csd->flush_trigger = MAX(csd->flush_trigger - MAX(csd->flush_trigger/STEP_FACTOR, 1), MIN_FLUSH_TRIGGER(csd->n_bts));
	if (0 == needed)
	{
		if (!is_mm)
		{	/* If another process is concurrently finishing up phase2 of commit, wait for that to complete first. */
			if (cr->in_tend && !wcs_phase2_commit_wait(csa, cr))
				return FALSE;	/* assumption is that caller will set wc_blocked and trigger cache recovery */
		}
		for (lcnt = 1; (MAXGETSPACEWAIT > lcnt) && (0 != cr->dirty); lcnt++)
		{	/* We want to flush a specific cache-record. We speed up the wait by moving the dirty cache-record
			 * to the head of the active queue. But to do this, we need exclusive access to the active queue.
			 * The only other processes outside of crit that can be touching this concurrently are wcs_wtstart
			 * (which can remove entries from the queue) and bg_update_phase2 (which can add entries to the queue).
			 * In the case of writers, we can wait for those to complete (by setting cnl->wc_blocked to TRUE)
			 * and then play with the queue. But in the case of bg_update_phase2, it is not easily possible to
			 * do a similar wait so in this case we choose to do plain wcs_wtstart (which uses interlocked
			 * queue operations and hence can work well with concurrent bg_update_phase2) and wait until the
			 * cache record of interest becomes non-dirty. The consequence is we might wait a little longer than
			 * necessary but that is considered acceptable for now.
			 */
			/* Check if cache recovery is needed (could be set by another process in
			 * secshr_db_clnup finishing off a phase2 commit). If so, no point invoking
			 * wcs_wtstart as it will return right away. Instead return FALSE so
			 * cache-recovery can be triggered by the caller.
			 */
			if (cnl->wc_blocked)
			{
				assert(gtm_white_box_test_case_enabled);
				return FALSE;
			}
			if (!is_mm && cnl->wcs_phase2_commit_pidcnt)
			{
				JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, dummy_errno);
					/* a macro that ensure jnl is open, dclast's wcs_wtstart and checks for errors etc. */
				wcs_sleep(lcnt);
			} else if (LATCH_CLEAR == WRITE_LATCH_VAL(cr))
			{
				SIGNAL_WRITERS_TO_STOP(cnl);	/* to stop all active writers */
				WAIT_FOR_WRITERS_TO_STOP(cnl, ocnt, MAXGETSPACEWAIT);
				if (MAXGETSPACEWAIT <= ocnt)
				{
					assert(FALSE);
					return FALSE;
				}
				if (LATCH_CLEAR == WRITE_LATCH_VAL(cr))
				{	/* Check if cache-record is part of the active queue. If so, then remove it from the
					 * tail of the active queue and move it to the head to try and speed up the flush.
					 * If not and if cr->dirty is non-zero, then the only way this is possible we know
					 * of is if a concurrent process encountered an error in the midst of commit in phase2
					 * of bg_update and finished the update but did not reinsert the cache-record in the
					 * active queue (see comment in secshr_db_clnup about why INSQ*I macros are not used
					 * in VMS). In this case, return FALSE as wcs_get_space cannot flush this cache-record.
					 * The caller will trigger appropriate error handling. We are guaranteed that cr cannot
					 * be part of the wip queue because WRITE_LATCH_VAL(cr) is LATCH_CLEAR (in wip queue it
					 * will be > LATCH_CLEAR).
					 */
					if (0 != cr->state_que.fl)
					{	/* We are about to play with the queues without using interlocks.
						 * Assert no one else could be concurrently playing with the queue.
						 */
						assert(!cnl->wcs_phase2_commit_pidcnt && !cnl->in_wtstart);
						base = &csa->acc_meth.bg.cache_state->cacheq_active;
						q0 = (que_ent_ptr_t)((sm_uc_ptr_t)&cr->state_que + cr->state_que.fl);
						shuffqth((que_ent_ptr_t)q0, (que_ent_ptr_t)base);
					} else if (cr->dirty)
					{
						assert(gtm_white_box_test_case_enabled);
						return FALSE;
					}
				}
				SIGNAL_WRITERS_TO_RESUME(cnl);
				JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, dummy_errno);
					/* a macro that ensure jnl is open, dclast's wcs_wtstart and checks for errors etc. */
				wcs_sleep(lcnt);
			} else if ((0 == cr->iosb.cond) || (WRT_STRT_PNDNG == cr->iosb.cond))
			{
				JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, dummy_errno);
					/* a macro that ensure jnl is open, dclast's wcs_wtstart and checks for errors etc. */
				wcs_sleep(lcnt);
			}
			if (FALSE == wcs_wtfini(reg))
				return FALSE;
		}
		if (0 == cr->dirty)
			return TRUE;
		assert(FALSE);
		return FALSE;
	}
	for (lcnt = 1; ((cnl->wc_in_free < needed) && (MAXGETSPACEWAIT > lcnt)); lcnt++)
	{
		DCLAST_WCS_WTSTART(reg, 0, dummy_errno); /* a macro that dclast's wcs_wtstart and checks for errors etc. */
		wcs_sleep(lcnt);
		if (FALSE == wcs_wtfini(reg))
			return FALSE;
	}
	if (cnl->wc_in_free < needed)
	{
		assert(FALSE);
		return FALSE;
	}
	return TRUE;
}
