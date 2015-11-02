/****************************************************************
 *
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "copy.h"
#include "util.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_opt_struct	mur_options;
GBLREF 	jnl_gbls_t	jgbl;

void mur_tp_resolve_time(jnl_tm_t max_lvrec_time)
{
	int 		reg_total;
	reg_ctl_list	*rctl, *rctl_top;
	jnl_tm_t	reg_tp_resolve_time;

	reg_total = murgbl.reg_total;
	if (mur_options.forward)
	{
		if (!mur_options.verify)
		{
			jgbl.mur_tp_resolve_time = mur_ctl[0].lvrec_time;
			for (rctl = mur_ctl + 1, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
			{
				if (rctl->lvrec_time < jgbl.mur_tp_resolve_time)
					jgbl.mur_tp_resolve_time = rctl->lvrec_time;
			}
			assert(jgbl.mur_tp_resolve_time);
		} else
			jgbl.mur_tp_resolve_time = 0; /* verify continues till the beginning of journal file */
		return;
	}
	mur_sort_files();
	jgbl.mur_tp_resolve_time = MAXUINT4;
	assert(max_lvrec_time == mur_ctl[reg_total - 1].lvrec_time);
	for (rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
	{
		/* Assumption : It is guaranteed to see an EPOCH in every
		 * "rctl->jctl->jfh->epoch_interval + MAX_EPOCH_DELAY" seconds. */
		assert(max_lvrec_time >= rctl->jctl->jfh->epoch_interval + MAX_EPOCH_DELAY);
		/* Calculate this region's TP resolve time based on the maximum of the last valid record across regions. If the
		 * region is properly closed (typically this means that the journal file's crash field is FALSE. But, with online
		 * rollback, crash field being TRUE, does not mean the journal file is crashed (as long as the shared memory for
		 * that region existed when the region was opened). So, use jctl->properly_closed to determine whether the journal
		 * file for the region is really crashed. If it is properly closed, the region's TP resolve time is the
		 * max_lvrec_time. If not, we need to go back by an epoch interval in addition to the MAX_EPOCH_DELAY.
		 */
		if (!rctl->jctl->properly_closed)
			reg_tp_resolve_time = max_lvrec_time - rctl->jctl->jfh->epoch_interval - MAX_EPOCH_DELAY;
		else
			reg_tp_resolve_time = max_lvrec_time;
		if (rctl->lvrec_time > reg_tp_resolve_time)
			reg_tp_resolve_time = rctl->lvrec_time;
		if (reg_tp_resolve_time < jgbl.mur_tp_resolve_time)
			jgbl.mur_tp_resolve_time = reg_tp_resolve_time;
		assert(!mur_options.update || NULL != rctl->csd);
		if (mur_options.update && rctl->recov_interrupted && rctl->csd->intrpt_recov_tp_resolve_time &&
				rctl->csd->intrpt_recov_tp_resolve_time < jgbl.mur_tp_resolve_time)
			/* Previous backward recovery/rollback was interrupted.
			 * Update tp_resolve_time to reflect the minimum of the previous and
			 * 	current recovery/rollback's turn-around-points.
			 * It is possible that both rctl->csd->intrpt_recov_resync_seqno and
			 * rctl->csd->intrpt_recov_tp_resolve_time are zero in case previous recover
			 * was killed after mur_open_files, (which sets csd->recov_interrupted)
			 * but before mur_back_process() which would have set csd->intrpt_recov_tp_resolve_time */
			jgbl.mur_tp_resolve_time = rctl->csd->intrpt_recov_tp_resolve_time;
	}
	if (mur_options.since_time < jgbl.mur_tp_resolve_time)
		jgbl.mur_tp_resolve_time = (jnl_tm_t)mur_options.since_time;
	if (FENCE_NONE == mur_options.fences && !mur_options.since_time_specified && !murgbl.intrpt_recovery)
		jgbl.mur_tp_resolve_time = max_lvrec_time;
}
