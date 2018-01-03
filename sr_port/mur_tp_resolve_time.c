/****************************************************************
 *
 * Copyright (c) 2005-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
	jnl_ctl_list	*jctl;
	jnl_tm_t	reg_tp_resolve_time;
	boolean_t	all_reg_before_image;

	reg_total = murgbl.reg_total;
	if (mur_options.forward)
	{
		if (mur_options.verify)
		{
			jgbl.mur_tp_resolve_time = 0; /* verify continues till the beginning of journal file */
			return;
		}
		/* Determine if ALL journal files across ALL regions have before-image journaling ON.
		 * If so, use the better tp_resolve_time calculation algorithm that takes into account
		 * an idle/free EPOCH gets written. If not, we have to stick with a pessimistic calculation.
		 */
		all_reg_before_image = TRUE;
		for (rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
		{
			if (!rctl->jctl->jfh->before_images)
			{
				all_reg_before_image = FALSE;
				break;
			}
		}
		if (!all_reg_before_image)
		{	/* Use pessimistic calculation */
			jgbl.mur_tp_resolve_time = mur_ctl[0].lvrec_time;
			for (rctl = mur_ctl + 1, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
			{
				if (rctl->lvrec_time < jgbl.mur_tp_resolve_time)
					jgbl.mur_tp_resolve_time = rctl->lvrec_time;
			}
			assert(jgbl.mur_tp_resolve_time);
			return;
		}
	}
	/* Use better tp_resolve_time calculation algorithm if all jnl files have BEFORE image journaling turned ON */
	mur_sort_files();
	jgbl.mur_tp_resolve_time = MAXUINT4;
	assert(max_lvrec_time == mur_ctl[reg_total - 1].lvrec_time);
	if ((FENCE_NONE == mur_options.fences) && !mur_options.since_time_specified && !murgbl.intrpt_recovery)
	{
		jgbl.mur_tp_resolve_time = max_lvrec_time;
		return;
	}
	for (rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
	{
		jctl = rctl->jctl;
		/* Assumption : It is guaranteed to see an EPOCH in every
		 * "jctl->jfh->epoch_interval + MAX_EPOCH_DELAY" seconds.
		 */
		assert(max_lvrec_time >= jctl->jfh->epoch_interval + MAX_EPOCH_DELAY);
		/* Calculate this region's TP resolve time based on the maximum of the last valid record across regions. If the
		 * region is properly closed (typically this means that the journal file's crash field is FALSE). But, with online
		 * rollback, crash field being TRUE, does not mean the journal file is crashed (as long as the shared memory for
		 * that region existed when the region was opened). So, use jctl->properly_closed to determine whether the journal
		 * file for the region is really crashed. If it is properly closed, the region's TP resolve time is the
		 * max_lvrec_time. If not, we need to go back by an epoch interval in addition to the MAX_EPOCH_DELAY.
		 * There is one exception to this rule and that is if the latest generation journal file of a region is properly
		 * closed but was once a previous generation journal file (possible for example if the user moves mumps.mjl out
		 * of the way and renames mumps.mjl_<timestamp> back as mumps.mjl). In that case, we want to treat the
		 * tp_resolve_time for this region as the last valid record timestamp in this journal file.
		 */
		if (!jctl->properly_closed)
			reg_tp_resolve_time = max_lvrec_time - jctl->jfh->epoch_interval -
				DEBUG_ONLY((WBTEST_ENABLED(WBTEST_LOWERED_JNLEPOCH))? gtm_wbox_input_test_case_count : )
					MAX_EPOCH_DELAY;
		else if (jctl->jfh->is_not_latest_jnl)
			reg_tp_resolve_time = rctl->lvrec_time;
		else
			reg_tp_resolve_time = max_lvrec_time;
		if (rctl->lvrec_time > reg_tp_resolve_time)
			reg_tp_resolve_time = rctl->lvrec_time;
		if (reg_tp_resolve_time < jgbl.mur_tp_resolve_time)
			jgbl.mur_tp_resolve_time = reg_tp_resolve_time;
		assert(!mur_options.update || (NULL != rctl->csd));
		if (!mur_options.forward && mur_options.update && rctl->recov_interrupted
				&& rctl->csd->intrpt_recov_tp_resolve_time
				&& (rctl->csd->intrpt_recov_tp_resolve_time < jgbl.mur_tp_resolve_time))
			/* Previous backward recovery/rollback was interrupted.
			 * Update tp_resolve_time to reflect the minimum of the previous and
			 * 	current recovery/rollback's turn-around-points.
			 * It is possible that both rctl->csd->intrpt_recov_resync_seqno and
			 * rctl->csd->intrpt_recov_tp_resolve_time are zero in case previous recover
			 * was killed after mur_open_files, (which sets csd->recov_interrupted)
			 * but before mur_back_process() which would have set csd->intrpt_recov_tp_resolve_time */
			jgbl.mur_tp_resolve_time = rctl->csd->intrpt_recov_tp_resolve_time;
	}
	if (!mur_options.forward && (mur_options.since_time < jgbl.mur_tp_resolve_time))
		jgbl.mur_tp_resolve_time = (jnl_tm_t)mur_options.since_time;
	return;
}
