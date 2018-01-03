/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "gt_timer.h"
#include "util.h"
#include "gv_rundown.h"
#include "print_exit_stats.h"
#include "repl_msg.h"		/* needed for jnlpool_addrs_ptr_t */
#include "gtmsource.h"		/* needed for jnlpool_addrs_ptr_t */
#include "secshr_db_clnup.h"
#include "gtmimagename.h"
#include "dpgbldir.h"
#include "gtmcrypt.h"

GBLREF	boolean_t	need_core;
GBLREF	boolean_t	created_core;
GBLREF	boolean_t	exit_handler_active;
GBLREF	boolean_t	skip_exit_handler;
GBLREF	uint4		dollar_tlevel;

void util_exit_handler()
{
	int		stat;
	gd_region	*r_top, *reg;
	sgmnt_addrs	*csa;
	gd_addr		*addr_ptr;

	if (exit_handler_active || skip_exit_handler) /* Skip exit handling if specified or if exit handler already active */
		return;
	exit_handler_active = TRUE;
	SET_PROCESS_EXITING_TRUE;	/* Set this BEFORE canceling timers as wcs_phase2_commit_wait relies on this */
	if (IS_DSE_IMAGE)
	{	/* Need to clear csa->hold_onto_crit in case it was set. This needs to be done before the call to
		 * secshr_db_clnup() which, if we still hold it, will take care of releasing crit at the appropriate point.
		 */
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
			{
				if (reg->open && !reg->was_open)
				{
					csa = &FILE_INFO(reg)->s_addrs;
					csa->hold_onto_crit = FALSE;	/* need to do this before the rel_crit */
					/* If this is an normal (non-error) exit (as determiend by the severity var), go ahead
					 * and release crit if we are holding it as that secshr_db_clnup() from forcing an
					 * unneeded cache recovery. However, if this *IS* an error condition, we leave crit
					 * alone and let secshr_db_clnup() deal with it appropriately.
					 */
					if ((0 == severity) && csa->now_crit)
						rel_crit(reg);
				}
			}
		}
	}
	CANCEL_TIMERS;		/* Cancel all unsafe timers - No unpleasant surprises */
	/* Note we call secshr_db_clnup() with the flag NORMAL_TERMINATION even in an error condition
	 * here because we know at this point that we aren't in the middle of a transaction but we may
	 * be holding crit in one or more regions and/or we could have other odds/ends to cleanup.
	 */
	secshr_db_clnup(NORMAL_TERMINATION);
	WITH_CH(exi_ch, gv_rundown(), 0);
	print_exit_stats();
	util_out_close();
	GTMCRYPT_CLOSE;
	if (need_core && !created_core)
		DUMP_CORE;
}
