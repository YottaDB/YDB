/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "secshr_db_clnup.h"
#include "gtmimagename.h"
#include "dpgbldir.h"

GBLREF	boolean_t	need_core;
GBLREF	boolean_t	created_core;
GBLREF	boolean_t	exit_handler_active;
GBLREF	uint4		dollar_tlevel;
GBLREF	boolean_t	hold_onto_crit;

void util_exit_handler()
{
	int		stat;
	gd_region	*r_top, *reg;
	sgmnt_addrs	*csa;
	gd_addr		*addr_ptr;

	if (exit_handler_active)	/* Don't recurse if exit handler exited */
		return;
	exit_handler_active = TRUE;
	SET_PROCESS_EXITING_TRUE;	/* set this BEFORE cancelling timers as wcs_phase2_commit_wait relies on this */
	if (IS_DSE_IMAGE)
	{	/* Need to clear csa->hold_onto_crit in case it was set */
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
			{
				if (reg->open && !reg->was_open)
				{
					csa = &FILE_INFO(reg)->s_addrs;
					csa->hold_onto_crit = FALSE;	/* need to do this before the rel_crit */
					if (csa->now_crit)
						rel_crit(reg);
				}
			}
		}
	}
	cancel_timer(0);		/* Cancel all timers - No unpleasant surprises */
	secshr_db_clnup(NORMAL_TERMINATION);
	assert(!dollar_tlevel);	/* MUPIP and GT.M are the only ones which can run TP and they have their own exit handlers.
				 * So no need to run op_trollback here like mupip_exit_handler and gtm_exit_handler. */
	gv_rundown();
	print_exit_stats();
	util_out_close();
	if (need_core && !created_core)
		DUMP_CORE;
}
